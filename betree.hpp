// A basic B^e-tree implementation templated on types Key and Value.
// Keys and Values must be serializable (see swap_space.hpp).
// Keys must be comparable (via operator< and operator==).
// Values must be addable (via operator+).
// See test.cpp for example usage.

// This implementation represents in-memory nodes as objects with two
// fields:
// - a std::map mapping keys to child pointers
// - a std::map mapping (key, timestamp) pairs to messages
// Nodes are de/serialized to/from an on-disk representation.
// I/O is managed transparently by a swap_space object.

// This implementation deviates from a "textbook" implementation in
// that there is not a fixed division of a node's space between pivots
// and buffered messages.

// In a textbook implementation, nodes have size B, B^e space is
// devoted to pivots and child pointers, and B-B^e space is devoted to
// buffering messages.  Whenever a leaf gets too many messages, it
// splits.  Whenever an internal node gets too many messages, it
// performs a flush.  Whenever an internal node gets too many
// children, it splits.  This policy ensures that, whenever the tree
// needs to flush messages from a node to one of its children, it can
// always move a batch of size at least (B-B^e) / B^e = B^(1-e) - 1
// messages.

// In this implementation, nodes have a fixed maximum size.  Whenever
// a leaf exceeds this max size, it splits.  Whenever an internal node
// exceeds this maximum size, it checks to see if it can flush a large
// batch of elements to one of its children.  If it can, it does so.
// If it cannot, then it splits.

// In-memory nodes may temporarily exceed the maximum size
// restriction.  During a flush, we move all the incoming messages
// into the destination node.  At that point the node may exceed the
// max size.  The flushing procedure then performs further flushes or
// splits to restore the max-size invariant.  Thus, whenever a flush
// returns, all the nodes in the subtree of that node are guaranteed
// to satisfy the max-size requirement.

// This implementation also optimizes I/O based on which nodes are
// on-disk, clean in memory, or dirty in memory.  For example,
// inserted items are always immediately flushed as far down the tree
// as they can go without dirtying any new nodes.  This is because
// flushing an item to a node that is already dirty will not require
// any additional I/O, since the node already has to be written back
// anyway.  Furthermore, it will flush smaller batches to clean
// in-memory nodes than to on-disk nodes.  This is because dirtying a
// clean in-memory node only requires a write-back, whereas flushing
// to an on-disk node requires reading it in and writing it out.

#include <cassert>
#include <cmath>
#include <math.h>
#include <map>
#include <math.h>
#include <cmath>
#include <vector>
#include <cassert>
#include <algorithm>

#include "swap_space.hpp"
#include "backing_store.hpp"
#include "window_stat_tracker.hpp"
////////////////// Upserts

// Internally, we store data indexed by both the user-specified key
// and a timestamp, so that we can apply upserts in the correct order.
template <class Key>
class MessageKey
{
public:
  MessageKey(void) : key(),
                     timestamp(0)
  {
  }

  MessageKey(const Key &k, uint64_t tstamp) : key(k),
                                              timestamp(tstamp)
  {
  }

  static MessageKey range_start(const Key &key)
  {
    return MessageKey(key, 0);
  }

  static MessageKey range_end(const Key &key)
  {
    return MessageKey(key, UINT64_MAX);
  }

  MessageKey range_start(void) const
  {
    return range_start(key);
  }

  MessageKey range_end(void) const
  {
    return range_end(key);
  }

  void _serialize(std::iostream &fs, serialization_context &context) const
  {
    fs << timestamp << " ";
    serialize(fs, context, key);
  }

  void _deserialize(std::iostream &fs, serialization_context &context)
  {
    fs >> timestamp;
    deserialize(fs, context, key);
  }

  Key key;
  uint64_t timestamp;
};

template <class Key>
bool operator<(const MessageKey<Key> &mkey1, const MessageKey<Key> &mkey2)
{
  return mkey1.key < mkey2.key ||
         (mkey1.key == mkey2.key && mkey1.timestamp < mkey2.timestamp);
}

template <class Key>
bool operator<(const Key &key, const MessageKey<Key> &mkey)
{
  return key < mkey.key;
}

template <class Key>
bool operator<(const MessageKey<Key> &mkey, const Key &key)
{
  return mkey.key < key;
}

template <class Key>
bool operator==(const MessageKey<Key> &a, const MessageKey<Key> &b)
{
  return a.key == b.key && a.timestamp == b.timestamp;
}

// The three types of upsert.  An UPDATE specifies a value, v, that
// will be added (using operator+) to the old value associated to some
// key in the tree.  If there is no old value associated with the key,
// then it will add v to the result of a Value obtained using the
// default zero-argument constructor.
#define INSERT (0)
#define DELETE (1)
#define UPDATE (2)

template <class Value>
class Message
{
public:
  Message(void) : opcode(INSERT),
                  val()
  {
  }

  Message(int opc, const Value &v) : opcode(opc),
                                     val(v)
  {
  }

  void _serialize(std::iostream &fs, serialization_context &context)
  {
    fs << opcode << " ";
    serialize(fs, context, val);
  }

  void _deserialize(std::iostream &fs, serialization_context &context)
  {
    fs >> opcode;
    deserialize(fs, context, val);
  }

  int opcode;
  Value val;
};

template <class Value>
bool operator==(const Message<Value> &a, const Message<Value> &b)
{
  return a.opcode == b.opcode && a.val == b.val;
}

// Measured in messages.
#define DEFAULT_MAX_NODE_SIZE (1ULL << 18)

// The minimum number of messages that we will flush to an out-of-cache node.
// Note: we will flush even a single element to a child that is already dirty.
// Note: we will flush MIN_FLUSH_SIZE/2 items to a clean in-memory child.
#define DEFAULT_MIN_FLUSH_SIZE (DEFAULT_MAX_NODE_SIZE / 16ULL)

template <class Key, class Value>
class betree
{
private:
  class node;
  // We let a swap_space handle all the I/O.
  typedef typename swap_space::pointer<node> node_pointer;
  class child_info : public serializable
  {
  public:
    child_info(void)
        : child(), child_size(0)
    {
    }

    child_info(node_pointer child, uint64_t child_size)
        : child(child),
          child_size(child_size)
    {
    }

    void _serialize(std::iostream &fs, serialization_context &context)
    {
      serialize(fs, context, child);
      fs << " ";
      serialize(fs, context, child_size);
    }

    void _deserialize(std::iostream &fs, serialization_context &context)
    {
      deserialize(fs, context, child);
      deserialize(fs, context, child_size);
    }

    node_pointer child;
    uint64_t child_size;
  };
  typedef typename std::map<Key, child_info> pivot_map;
  typedef typename std::map<MessageKey<Key>, Message<Value>> message_map;

  class node : public serializable
  {
  private:
    // Init class for sliding window statistic tracker on the Tree
    // with default value for W value (size of sliding window)
    window_stat_tracker stat_tracker;

  public:
    // Child pointers
    pivot_map pivots;
    message_map elements;
    // Size of node to base other parameters on
    uint64_t max_node_size;
    uint64_t min_node_size;
    uint64_t min_flush_size;
    // Node specific parameters
    float epsilon;
    uint64_t max_pivots;
    uint64_t max_messages;
    uint64_t node_level;
    uint64_t operation_count;
    uint64_t const ops_before_epsilon_update;
    uint64_t const window_size;
    uint64_t node_id;
    bool ready_for_adoption = false;

    node()
        : max_node_size(64), min_node_size(64 / 4), min_flush_size(64 / 16), epsilon(0.4), node_level(0), operation_count(0), ops_before_epsilon_update(100), window_size(100), node_id(-1)
    {
      max_pivots = calculate_max_pivots();
      max_messages = max_node_size - max_pivots;
      stat_tracker = window_stat_tracker();
    }

    node(float e, uint64_t level, uint64_t opsbeforeupdate = 100, uint64_t windowsize = 100)
        : max_node_size(64), min_node_size(64 / 4), min_flush_size(64 / 16), epsilon(e), node_level(level), operation_count(0), ops_before_epsilon_update(opsbeforeupdate), window_size(windowsize), node_id(-1)
    {
      max_pivots = calculate_max_pivots();
      max_messages = max_node_size - max_pivots;
      stat_tracker = window_stat_tracker(window_size);
    }

    uint64_t get_node_id()
    {
      return node_id;
    }
    void set_node_id(uint64_t new_id)
    {
      node_id = new_id;
    }

    uint64_t calculate_max_pivots()
    {

      // Calculate B^(Epsilon)
      float B_eps = pow(max_node_size, epsilon);
      uint64_t B = (uint64_t)round(B_eps);

      // Set it to the nearest multiple of 4
      int remainder = B % 4;
      int max_pivots;

      if (remainder < 2)
      {
        max_pivots = B - remainder;
      }
      else if (remainder == 2)
      {
        if (B > 32)
        {
          // round up to nearest 4 multiple
          // for optimizing towards reads
          max_pivots = B + remainder;
        }
        else
        {
          // round down to nearest 4 multiple
          // for optimizing towards rights
          max_pivots = B - remainder;
        }
      }
      else
      {
        max_pivots = B + (4 - remainder);
      }

      return max_pivots;
    }

    // set epsilon, max_messages, and max_pivots for this node
    void set_epsilon(float e, betree &bet)
    {
      auto prev_max_pivots = max_pivots;

      epsilon = e;
      max_pivots = calculate_max_pivots();
      max_messages = max_node_size - max_pivots;

      if (max_pivots != prev_max_pivots && node_level == bet.tunable_epsilon_level && bet.is_dynamic) {
	   recursive_set_epsilon(bet, max_pivots, max_messages, epsilon); // update 
      }

      // flag node as ready for adoption if max_pivots increases after epsilon update
      if (max_pivots > prev_max_pivots)
      {
	if (node_level == bet.tunable_epsilon_level){
	  
	  flag_as_ready_for_adoption_recursive(bet);// flag all nodes below this one
	}
	else if (node_level < bet.tunable_epsilon_level) {
          ready_for_adoption = true;
        }
      }
    }

    // decrement node_level when adopted
    void decrement_node_level()
    {
      node_level--;
    }

    // add single read count to window stat tracker on this node
    void add_read(betree &bet)
    {
      stat_tracker.add_read();
      operation_count += 1;
      // periodically update epsilon
      if (operation_count == ops_before_epsilon_update)
      {
        float new_epsilon = stat_tracker.get_epsilon();
        set_epsilon(new_epsilon, bet);
        operation_count = 0;
      }
    }

    // add single write count to window stat tracker on this node
    void add_write(betree &bet)
    {
      stat_tracker.add_write();
      operation_count += 1;
      // periodically update epsilon
      if (operation_count == ops_before_epsilon_update)
      {
        float new_epsilon = stat_tracker.get_epsilon();
        set_epsilon(new_epsilon, bet);
        operation_count = 0;
      }
    }

    bool is_leaf(void) const
    {
      return pivots.empty();
    }

    // Holy frick-a-moly.  We want to write a const function that
    // returns a const_iterator when called from a const function and
    // a non-const function that returns a (non-const_)iterator when
    // called from a non-const function.  And we don't want to
    // duplicate the code.  The following solution is from
    //         http://stackoverflow.com/a/858893
    template <class OUT, class IN>
    static OUT get_pivot(IN &mp, const Key &k)
    {
      assert(mp.size() > 0);
      auto it = mp.lower_bound(k);
      if (it == mp.begin() && k < it->first)
        throw std::out_of_range("Key does not exist "
                                "(it is smaller than any key in DB)");
      if (it == mp.end() || k < it->first)
        --it;
      return it;
    }

    // Instantiate the above template for const and non-const
    // calls. (template inference doesn't seem to work on this code)
    typename pivot_map::const_iterator get_pivot(const Key &k) const
    {
      return get_pivot<typename pivot_map::const_iterator,
                       const pivot_map>(pivots, k);
    }

    typename pivot_map::iterator
    get_pivot(const Key &k)
    {
      return get_pivot<typename pivot_map::iterator, pivot_map>(pivots, k);
    }

    // Return iterator pointing to the first element with mk >= k.
    // (Same const/non-const templating trick as above)
    template <class OUT, class IN>
    static OUT get_element_begin(IN &elts, const Key &k)
    {
      return elts.lower_bound(MessageKey<Key>::range_start(k));
    }

    typename message_map::iterator get_element_begin(const Key &k)
    {
      return get_element_begin<typename message_map::iterator,
                               message_map>(elements, k);
    }

    typename message_map::const_iterator get_element_begin(const Key &k) const
    {
      return get_element_begin<typename message_map::const_iterator,
                               const message_map>(elements, k);
    }

    // Return iterator pointing to the first element that goes to
    // child indicated by it
    typename message_map::iterator
    get_element_begin(const typename pivot_map::iterator it)
    {
      return it == pivots.end() ? elements.end() : get_element_begin(it->first);
    }

    // Apply a message to ourself.
    void apply(const MessageKey<Key> &mkey, const Message<Value> &elt,
               Value &default_value)
    {
      switch (elt.opcode)
      {
      case INSERT:
        elements.erase(elements.lower_bound(mkey.range_start()),
                       elements.upper_bound(mkey.range_end()));
        elements[mkey] = elt;
        break;

      case DELETE:
        elements.erase(elements.lower_bound(mkey.range_start()),
                       elements.upper_bound(mkey.range_end()));
        if (!is_leaf())
          elements[mkey] = elt;
        break;

      case UPDATE:
      {
        auto iter = elements.upper_bound(mkey.range_end());
        if (iter != elements.begin())
          iter--;
        if (iter == elements.end() || iter->first.key != mkey.key)
          if (is_leaf())
          {
            Value dummy = default_value;
            apply(mkey, Message<Value>(INSERT, dummy + elt.val),
                  default_value);
          }
          else
          {
            elements[mkey] = elt;
          }
        else
        {
          assert(iter != elements.end() && iter->first.key == mkey.key);
          if (iter->second.opcode == INSERT)
          {
            apply(mkey, Message<Value>(INSERT, iter->second.val + elt.val),
                  default_value);
          }
          else
          {
            elements[mkey] = elt;
          }
        }
      }
      break;

      default:
        assert(0);
      }
    }

    // Method to shorten parts of the tree.
    //
    // This method goes through the children of the node its called on and will adopt
    // grandchildren and erase their parents. A node can only adopt up to their max_pivots
    // amount of grandchildren. Grandchildren will only be adopted if all of the siblings
    // in the families of grandchildren can be adopted. If sibling grandchildren are adopted,
    // their parent (child of this node) is killed.
    //
    // Currently, if grandchildren are adopted and their parents are killed, all of the elements
    // in those parents (former children of this node) will be forwarded and applied to the
    // grandchildren being adopted. The adopted children may have > max_messges after this forwarding
    // process (they will be naturally handled by a flush later).
    //
    // -----------------------------------------------------------------------------------------
    void adopt(betree &bet)
    {
      // Nothing to adopt if leaf
      if (is_leaf())
      {
        ready_for_adoption = false;
        return;
      }

      // No need to adopt if pivots at max
      if (pivots.size() >= max_pivots)
      {
        ready_for_adoption = false;
        return;
      }

      uint64_t total_pivots = pivots.size();

      // First get all the IDs of current children to potentially kill
      std::vector<uint64_t> cur_child_ids;
      for (auto it = pivots.begin(); it != pivots.end(); ++it)
      {
        uint64_t cur_id = it->second.child->get_node_id();
        cur_child_ids.push_back(cur_id);
      }
      uint64_t size_before_adopt = cur_child_ids.size();

      // Iterate over children and count their pivots to assess if
      // we can adopt grandchildren from them
      for (uint64_t i = 0; i < size_before_adopt; i++)
      {
        // iterate till we find the child with node_id == cur_child_ids[0] or reach the end
        auto it = pivots.begin();
        while (true)
        {
          auto cur_id = it->second.child->get_node_id();
          if (cur_id == cur_child_ids[0])
          {
            break;
          }
          it = next(it);
          if (it == pivots.end())
          {
            break;
          }
        }

        if (it != pivots.end())
        { // if not at end

          // see if we can adopt all sibling grandchildren
          if (((total_pivots - 1) + it->second.child->pivots.size()) > max_pivots)
          {
            continue; // don't adopt the set of grandchildren if it would result in > max_pivots
          }

          auto child_to_erase = it->second.child; // the child whose grandchildren we'll adopt

          // Skip if child is leaf, there's no grandchildren to adopt
          if (child_to_erase->is_leaf())
          {
            continue;
          }

          pivot_map grandchildren = child_to_erase->pivots; // granchildren of the child

          if (grandchildren.size() > 0)
          {

    	    // apply all messages to this node
	    message_map child_messages = child_to_erase->elements;
	    for (auto eltit = child_messages.begin(); eltit != child_messages.end(); ++eltit) {
		apply(eltit->first, eltit->second, bet.default_value);
	    }

            // kill child
            pivots.erase(it);
            child_to_erase->pivots.clear();
            child_to_erase->elements.clear();

            // decrement node_level of adoptees
            for (auto adopt_it = grandchildren.begin(); adopt_it != grandchildren.end(); ++adopt_it)
            {
              adopt_it->second.child->decrement_node_level();
            }

            // adopt sibling grandchildren
            pivots.insert(grandchildren.begin(), grandchildren.end());

            // remove element at 0 index of cur_child_ids and push everything back
            // to assess the next child that isn't adopted
            cur_child_ids.erase(cur_child_ids.begin());

            // update pivot count
            total_pivots = pivots.size();
          }
        }
      }

      // After adoption, go through all children of this node and udpates child_size
      for (auto it = pivots.begin(); it != pivots.end(); ++it)
      {
        it->second.child_size =
            it->second.child->pivots.size() +
            it->second.child->elements.size();
      }

      ready_for_adoption = false;
    }
    // --------------------------------------------------------------- //


    // Requires: there are less than MIN_FLUSH_SIZE things in elements
    //           destined for each child in pivots);
    pivot_map split(betree &bet)
    {
      // UPDATED ASSERT doesn't check against max size of node.
      // This checks that at least the pivots or messages are outside of their bounds.
      assert((pivots.size() >= max_pivots) || (elements.size() >= max_messages));
      // This size split does a good job of causing the resulting
      // nodes to have size between 0.4 * MAX_NODE_SIZE and 0.6 * MAX_NODE_SIZE.
      int num_new_leaves = (pivots.size() + elements.size()) / (10 * max_node_size / 24);
      if (num_new_leaves < 2)
      {
        num_new_leaves = 2;
      }
      // Make sure nothing here is left after adding to leaves
      int things_per_new_leaf =
          (pivots.size() + elements.size() + num_new_leaves - 1) / num_new_leaves;

      pivot_map result;
      auto pivot_idx = pivots.begin();
      auto elt_idx = elements.begin();
      int things_moved = 0;
      // Iterate through number of new leaves to move items from this node in
      // Each leaf is a new node allocated and added to result pivot_map
      for (int i = 0; i < num_new_leaves; i++)
      {
        if (pivot_idx == pivots.end() && elt_idx == elements.end())
          // Moved all pivots and elements to new leaves
          break;
        // Allocate a new node
        auto e = epsilon;
        auto l = node_level + 1;

        node_pointer new_node = bet.ss->allocate(new node(e, l, bet.ops_before_update, bet.window_size));

        auto new_node_id = bet.glob_id_inc++;
        new_node->set_node_id(new_node_id);

        // If there are still pivots to move...
        // result[pivot_idx->first] = child_info(new_node, 0 + 0)
        // Else if looping through elements...
        // result[elt_idx->first.key] = child_info(new_node, 0 + 0)
        result[pivot_idx != pivots.end() ? pivot_idx->first : elt_idx->first.key] = child_info(new_node,
                                                                                               new_node->elements.size() +
                                                                                                   new_node->pivots.size());
        // While there are still things to move to this leaf
        while (things_moved < (i + 1) * things_per_new_leaf &&
               (pivot_idx != pivots.end() || elt_idx != elements.end()))
        {
          // Move pivots
          if (pivot_idx != pivots.end())
          {
            // Add pivot to new node
            new_node->pivots[pivot_idx->first] = pivot_idx->second;
            // Increment the pivot idx so we know when to stop adding elements
            // and don't add elements from the next pivot
            ++pivot_idx;
            things_moved++;
            auto elt_end = get_element_begin(pivot_idx);
            // Get all elements for this pivot and move them to the node
            while (elt_idx != elt_end)
            {
              new_node->elements[elt_idx->first] = elt_idx->second;
              ++elt_idx;
              things_moved++;
            }
          }
          else
          {
            // Must be a leaf. Move elements up to things_per_new_leaf into the new node.
            assert(pivots.size() == 0);
            new_node->elements[elt_idx->first] = elt_idx->second;
            ++elt_idx;
            things_moved++;
          }
        }
      }

      for (auto it = result.begin(); it != result.end(); ++it)
        it->second.child_size = it->second.child->elements.size() +
                                it->second.child->pivots.size();

      assert(pivot_idx == pivots.end());
      assert(elt_idx == elements.end());
      pivots.clear();
      elements.clear();
      return result;
    }

    node_pointer merge(betree &bet,
                       typename pivot_map::iterator begin,
                       typename pivot_map::iterator end)
    {
      auto e = epsilon;
      auto l = node_level;
      node_pointer new_node = bet.ss->allocate(new node(e, l, bet.ops_before_update, bet.window_size));
      auto new_node_id = bet.glob_id_inc++;
      new_node->set_node_id(new_node_id);
      for (auto it = begin; it != end; ++it)
      {
        new_node->elements.insert(it->second.child->elements.begin(),
                                  it->second.child->elements.end());
        new_node->pivots.insert(it->second.child->pivots.begin(),
                                it->second.child->pivots.end());
      }
      return new_node;
    }

    void merge_small_children(betree &bet)
    {
      if (is_leaf())
        return;

      for (auto beginit = pivots.begin(); beginit != pivots.end(); ++beginit)
      {
        uint64_t total_size = 0;
        auto endit = beginit;
        while (endit != pivots.end())
        {
          if (total_size + beginit->second.child_size > 6 * bet.max_node_size / 10)
            break;
          total_size += beginit->second.child_size;
          ++endit;
        }
        if (endit != beginit)
        {
          node_pointer merged_node = merge(bet, beginit, endit);
          for (auto tmp = beginit; tmp != endit; ++tmp)
          {
            tmp->second.child->elements.clear();
            tmp->second.child->pivots.clear();
          }
          Key key = beginit->first;
          pivots.erase(beginit, endit);
          pivots[key] = child_info(merged_node, merged_node->pivots.size() + merged_node->elements.size());
          beginit = pivots.lower_bound(key);
        }
      }
    }

    // Recursive method that flags this node and all children, grandchildren, etc as
    // ready_for_adoption
    void flag_as_ready_for_adoption_recursive(betree &bet){
      // Recurse down to bottom of tree
      for (auto it = pivots.begin(); it != pivots.end(); ++it)
      {
        it->second.child->flag_as_ready_for_adoption_recursive(bet);
      }
	
      ready_for_adoption = true;
    }

    // Sets new epsilon, max_pivots, and max_messages on all nodes below and including the one is is originally 
    // called on
    void recursive_set_epsilon(betree &bet, uint64_t new_mav_pivots, uint64_t new_max_messages, float eps) {
	
	// recurse down
	for (auto it = pivots.begin(); it != pivots.end(); ++it)
        {
	   it->second.child->recursive_set_epsilon(bet, new_mav_pivots, new_max_messages, eps);
	}

	// update
	epsilon = eps;
	max_pivots = new_mav_pivots;
	max_messages = new_max_messages;
    }


    // recursive method to return the height of the tree
    int tree_height_recursive(betree &bet, int currentLevel = 0)
    {
      if (is_leaf())
      {
        return currentLevel;
      }

      std::vector<int> heights;
      for (auto &pivot : pivots)
      {
        int height = pivot.second.child->tree_height_recursive(bet, currentLevel + 1);
        heights.push_back(height);
      }
      // Return max height
      return *std::max_element(heights.begin(), heights.end());
    }

    // recursive method to count all the nodes in the tree
    int node_count_recursive(betree &bet)
    {
      int count = 1; // current node
      for (auto &pivot : pivots)
      {
        count += pivot.second.child->node_count_recursive(bet);
      }
      return count;
    }
    // recursive method to count all the pivots in the tree
    int pivot_count_recursive(betree &bet)
    {
      int count = 0;
      for (auto &pivot : pivots)
      {
        count += pivot.second.child->pivot_count_recursive(bet);
      }
      return count;
    }
    // Print the amount of messages in each node
    void message_count_recursive(betree &bet)
    {
      if (is_leaf())
      {
        std::cout << "leaf messages: " << std::to_string(elements.size()) << std::endl;
        return;
      }
      else
      {
        std::cout << "messages: " << std::to_string(elements.size()) << std::endl;
        for (auto &pivot : pivots)
        {
          pivot.second.child->message_count_recursive(bet);
        }
      }
    }

    // Receive a collection of new messages and perform recursive
    // flushes or splits as necessary.  If we split, return a
    // map with the new pivot keys pointing to the new nodes.
    // Otherwise return an empty map.
    pivot_map flush(betree &bet, message_map &elts)
    {
      // If this node is less than the tunable epsilon tree level
      // Checks for an epsilon update.
      if (bet.is_dynamic && node_level <= bet.tunable_epsilon_level)
      {
        add_write(bet);
      }

      // REMEMBER
      // If too many messages, we need to flush.
      // If too many pivots, we need to split.
      debug(std::cout << "Flushing " << this << std::endl);
      pivot_map result;

      if (elts.size() == 0)
      {
        debug(std::cout << "Done (empty input)" << std::endl);
        return result;
      }

      // Leaves care about messages. Split if this leaf has too many.
      if (is_leaf())
      {
        for (auto it = elts.begin(); it != elts.end(); ++it)
          apply(it->first, it->second, bet.default_value);
        // Leaves don't contain pivots, so only need to check max message size.
        if (elements.size() >= max_messages)
          result = split(bet);
        return result;
      }

      ////////////// Non-leaf

      // Update the key of the first child, if necessary
      Key oldmin = pivots.begin()->first;
      MessageKey<Key> newmin = elts.begin()->first;
      if (newmin < oldmin)
      {
        pivots[newmin.key] = pivots[oldmin];
        pivots.erase(oldmin);
      }

      // If everything is going to a single dirty child, go ahead
      // and put it there.
      auto first_pivot_idx = get_pivot(elts.begin()->first.key);
      auto last_pivot_idx = get_pivot((--elts.end())->first.key);
      if (first_pivot_idx == last_pivot_idx &&
          first_pivot_idx->second.child.is_dirty())
      {
        // There shouldn't be anything in our buffer for this child,
        // but lets assert that just to be safe.
        {
          auto next_pivot_idx = next(first_pivot_idx);
          auto elt_start = get_element_begin(first_pivot_idx);
          auto elt_end = get_element_begin(next_pivot_idx);
          //assert(elt_start == elt_end);
        }
        // Flush the messages from further down the tree.
        pivot_map new_children = first_pivot_idx->second.child->flush(bet, elts);
	
	// If more leaves were created from the flush, update our pivots.
        if (!new_children.empty())
        {
          pivots.erase(first_pivot_idx);
          pivots.insert(new_children.begin(), new_children.end());
        }
        // Otherwise, make sure the size of the node we flushed to is up to date.
        else
        {
          first_pivot_idx->second.child_size =
              first_pivot_idx->second.child->pivots.size() +
              first_pivot_idx->second.child->elements.size();
        }
      }
      else
      {
        // Apply each message in our elements to ourself (meaning this node)
        for (auto it = elts.begin(); it != elts.end(); ++it)
          apply(it->first, it->second, bet.default_value);

        // Now flush to out-of-core or clean children as necessary
        while (elements.size() >= max_messages || pivots.size() >= max_pivots)
        {
          // Find the child with the largest set of messages in our buffer
          unsigned int max_size = 0;
          auto child_pivot = pivots.begin();
          auto next_pivot = pivots.begin();
          for (auto it = pivots.begin(); it != pivots.end(); ++it)
          {
            auto it2 = next(it);
            auto elt_it = get_element_begin(it);
            auto elt_it2 = get_element_begin(it2);
            unsigned int dist = distance(elt_it, elt_it2);
            if (dist > max_size)
            {
              child_pivot = it;
              next_pivot = it2;
              max_size = dist;
            }
          }
          // If one of these conditions is false, we have too many pivots
          // 1. the max node size is greater than the min flush size
          // 2. the max node size is not bigger than half the min flush size and the child is in memory
          if (!(max_size > min_flush_size ||
                (max_size > min_flush_size / 2 && child_pivot->second.child.is_in_memory())))
          {
            break; // We need to split because we have too many pivots
          }

          auto elt_child_it = get_element_begin(child_pivot);
          auto elt_next_it = get_element_begin(next_pivot);
          message_map child_elts(elt_child_it, elt_next_it);
          
          pivot_map new_children = child_pivot->second.child->flush(bet, child_elts);
	  
	  elements.erase(elt_child_it, elt_next_it);
          if (!new_children.empty())
          {
            // Update the pivots.
            pivots.erase(child_pivot);
            pivots.insert(new_children.begin(), new_children.end());
          }
          else
          {
            // Otherwise if there are no new nodes, make sure the node size is up to date.
            first_pivot_idx->second.child_size =
                child_pivot->second.child->pivots.size() +
                child_pivot->second.child->elements.size();
          }
        }

        // We have too many pivots to efficiently flush stuff down, so split
        // This is checked on every flush that isn't on a leaf node
        if (pivots.size() > max_pivots)
        {
          result = split(bet);
        }
      }

      // merge_small_children(bet);

      debug(std::cout << "Done flushing " << this << std::endl);
      return result;
    }

    Value query(betree &bet, const Key k)
    {
      debug(std::cout << "Querying " << this << std::endl);
      // If this node is less than the tunable epsilon tree level
      // Checks for an epsilon update.
      if (bet.is_dynamic && node_level <= bet.tunable_epsilon_level)
      {
        add_read(bet);
      }
      if (is_leaf())
      {
        auto it = elements.lower_bound(MessageKey<Key>::range_start(k));
        if (it != elements.end() && it->first.key == k)
        {
          assert(it->second.opcode == INSERT);
          return it->second.val;
        }
        else
        {
          throw std::out_of_range("Key does not exist");
        }
      }

      ///////////// Non-leaf

      auto message_iter = get_element_begin(k);
      Value v = bet.default_value;

      if (message_iter == elements.end() || k < message_iter->first)
        // If we don't have any messages for this key, just search
        // further down the tree.
        v = get_pivot(k)->second.child->query(bet, k);
      else if (message_iter->second.opcode == UPDATE)
      {
        // We have some updates for this key.  Search down the tree.
        // If it has something, then apply our updates to that.  If it
        // doesn't have anything, then apply our updates to the
        // default initial value.
        try
        {
          Value t = get_pivot(k)->second.child->query(bet, k);
          v = t;
        }
        catch (std::out_of_range &e)
        {
        }
      }
      else if (message_iter->second.opcode == DELETE)
      {
        // We have a delete message, so we don't need to look further
        // down the tree.  If we don't have any further update or
        // insert messages, then we should return does-not-exist (in
        // this subtree).
        message_iter++;
        if (message_iter == elements.end() || k < message_iter->first)
          throw std::out_of_range("Key does not exist");
      }
      else if (message_iter->second.opcode == INSERT)
      {
        // We have an insert message, so we don't need to look further
        // down the tree.  We'll apply any updates to this value.
        v = message_iter->second.val;
        message_iter++;
      }

      // Apply any updates to the value obtained above.
      while (message_iter != elements.end() && message_iter->first.key == k)
      {
        assert(message_iter->second.opcode == UPDATE);
        v = v + message_iter->second.val;
        message_iter++;
      }

      // Node adopts after query if ready to shorten tree
      if (ready_for_adoption){
        adopt(bet);
      }

      return v;
    }

    std::pair<MessageKey<Key>, Message<Value>>
    get_next_message_from_children(const MessageKey<Key> *mkey) const
    {
      if (mkey && *mkey < pivots.begin()->first)
        mkey = NULL;
      auto it = mkey ? get_pivot(mkey->key) : pivots.begin();
      while (it != pivots.end())
      {
        try
        {
          return it->second.child->get_next_message(mkey);
        }
        catch (std::out_of_range &e)
        {
        }
        ++it;
      }
      throw std::out_of_range("No more messages in any children");
    }

    std::pair<MessageKey<Key>, Message<Value>>
    get_next_message(const MessageKey<Key> *mkey) const
    {
      auto it = mkey ? elements.upper_bound(*mkey) : elements.begin();

      if (is_leaf())
      {
        if (it == elements.end())
          throw std::out_of_range("No more messages in sub-tree");
        return std::make_pair(it->first, it->second);
      }

      if (it == elements.end())
        return get_next_message_from_children(mkey);

      try
      {
        auto kids = get_next_message_from_children(mkey);
        if (kids.first < it->first)
          return kids;
        else
          return std::make_pair(it->first, it->second);
      }
      catch (std::out_of_range &e)
      {
        return std::make_pair(it->first, it->second);
      }
    }

    void _serialize(std::iostream &fs, serialization_context &context)
    {
      fs << "pivots:" << std::endl;
      serialize(fs, context, pivots);
      fs << "elements:" << std::endl;
      serialize(fs, context, elements);
      fs << "epsilon: ";
      serialize(fs, context, epsilon);
      fs << "\nnode_level: ";
      serialize(fs, context, node_level);
      fs << "\nnode_id: ";
      serialize(fs, context, node_id);
      fs << "\nready_for_adoption: ";
      serialize(fs, context, ready_for_adoption);
    }

    void _deserialize(std::iostream &fs, serialization_context &context)
    {
      std::string dummy;
      fs >> dummy;
      deserialize(fs, context, pivots);
      fs >> dummy;
      deserialize(fs, context, elements);
      fs >> dummy;
      deserialize(fs, context, epsilon);
      fs >> dummy;
      deserialize(fs, context, node_level);
      fs >> dummy;
      deserialize(fs, context, node_id);
      fs >> dummy;
      deserialize(fs, context, ready_for_adoption);
    }
  };

  swap_space *ss;
  uint64_t min_flush_size;
  uint64_t max_node_size;
  uint64_t min_node_size;
  bool is_dynamic;
  node_pointer root;
  uint64_t next_timestamp = 1; // Nothing has a timestamp of 0
  Value default_value;
  float const starting_epsilon;
  uint64_t tunable_epsilon_level;
  uint64_t const ops_before_update;
  uint64_t const window_size;
  uint64_t glob_id_inc = 0;

public:
  betree(swap_space *sspace,
         uint64_t maxnodesize = 64,
         uint64_t minnodesize = 64 / 4,
         uint64_t minflushsize = 64 / 16,
         bool isdynamic = false,
         float startingepsilon = 0.4, 
         uint64_t tunableepsilonlevel = 0,
         uint64_t opsbeforeupdate = 100,
         uint64_t windowsize = 100)
      : ss(sspace),
        max_node_size(maxnodesize),
        min_node_size(minnodesize),
        min_flush_size(minflushsize),
        is_dynamic(isdynamic),
        starting_epsilon(startingepsilon),
        tunable_epsilon_level(tunableepsilonlevel),
        ops_before_update(opsbeforeupdate),
        window_size(windowsize)
  {
    // The root is always at level 0 in the tree.
    root = ss->allocate(new node(starting_epsilon, 0, ops_before_update, window_size));
    auto new_node_id = glob_id_inc++; // init node_id
    root->set_node_id(new_node_id);
  }

  // Wrapper methods to call recursive methods to
  // get Tree stats
  int get_tree_height()
  {
    return root->tree_height_recursive(*this, 0);
  }
  int get_node_count()
  {
    return root->node_count_recursive(*this);
  }
  int get_pivot_count()
  {
    return root->pivot_count_recursive(*this);
  }
  void print_message_count_in_nodes()
  {
    root->message_count_recursive(*this);
  }

  // Insert the specified message and handle a split of the root if it
  // occurs.
  void upsert(int opcode, Key k, Value v)
  {
    message_map tmp;
    tmp[MessageKey<Key>(k, next_timestamp++)] = Message<Value>(opcode, v);
    pivot_map new_nodes = root->flush(*this, tmp);
    
    if (new_nodes.size() > 0)
    {
      auto e = root->epsilon;

      // The root's level should always be 0
      root = ss->allocate(new node(e, 0, ops_before_update, window_size));
      root->pivots = new_nodes;

      // set new node_id
      auto new_node_id = glob_id_inc++;
      root->set_node_id(new_node_id);
    }
  }

  void insert(Key k, Value v)
  {
    upsert(INSERT, k, v);
  }

  void update(Key k, Value v)
  {
    upsert(UPDATE, k, v);
  }

  void erase(Key k)
  {
    upsert(DELETE, k, default_value);
  }

  Value query(Key k)
  {
    Value v = root->query(*this, k);

    return v;
  }

  void dump_messages(void)
  {
    std::pair<MessageKey<Key>, Message<Value>> current;

    std::cout << "############### BEGIN DUMP ##############" << std::endl;

    try
    {
      current = root->get_next_message(NULL);
      do
      {
        std::cout << current.first.key << " "
                  << current.first.timestamp << " "
                  << current.second.opcode << " "
                  << current.second.val << std::endl;
        current = root->get_next_message(&current.first);
      } while (1);
    }
    catch (std::out_of_range e)
    {
    }
  }

  class iterator
  {
  public:
    iterator(const betree &bet)
        : bet(bet),
          position(),
          is_valid(false),
          pos_is_valid(false),
          first(),
          second()
    {
    }

    iterator(const betree &bet, const MessageKey<Key> *mkey)
        : bet(bet),
          position(),
          is_valid(false),
          pos_is_valid(false),
          first(),
          second()
    {
      try
      {
        position = bet.root->get_next_message(mkey);
        pos_is_valid = true;
        setup_next_element();
      }
      catch (std::out_of_range &e)
      {
      }
    }

    void apply(const MessageKey<Key> &msgkey, const Message<Value> &msg)
    {
      switch (msg.opcode)
      {
      case INSERT:
        first = msgkey.key;
        second = msg.val;
        is_valid = true;
        break;
      case UPDATE:
        first = msgkey.key;
        if (is_valid == false)
          second = bet.default_value;
        second = second + msg.val;
        is_valid = true;
        break;
      case DELETE:
        is_valid = false;
        break;
      default:
        abort();
        break;
      }
    }

    void setup_next_element(void)
    {
      is_valid = false;
      while (pos_is_valid && (!is_valid || position.first.key == first))
      {
        apply(position.first, position.second);
        try
        {
          position = bet.root->get_next_message(&position.first);
        }
        catch (std::exception &e)
        {
          pos_is_valid = false;
        }
      }
    }

    bool operator==(const iterator &other)
    {
      return &bet == &other.bet &&
             is_valid == other.is_valid &&
             pos_is_valid == other.pos_is_valid &&
             (!pos_is_valid || position == other.position) &&
             (!is_valid || (first == other.first && second == other.second));
    }

    bool operator!=(const iterator &other)
    {
      return !operator==(other);
    }

    iterator &operator++(void)
    {
      setup_next_element();
      return *this;
    }

    const betree &bet;
    std::pair<MessageKey<Key>, Message<Value>> position;
    bool is_valid;
    bool pos_is_valid;
    Key first;
    Value second;
  };

  iterator begin(void) const
  {
    return iterator(*this, NULL);
  }

  iterator lower_bound(Key key) const
  {
    MessageKey<Key> tmp = MessageKey<Key>::range_start(key);
    return iterator(*this, &tmp);
  }

  iterator upper_bound(Key key) const
  {
    MessageKey<Key> tmp = MessageKey<Key>::range_end(key);
    return iterator(*this, &tmp);
  }

  iterator end(void) const
  {
    return iterator(*this);
  }
};
