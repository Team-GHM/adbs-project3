// Author: Marcus, Adair, u1131818
//
// File to define a sliding window statistic tracker for the Tree.
// A Tree will have one of these as a private member variable.
// A window_stat_tracker tracks the most recent W reads/writes on the Tree,
// performs functions with those stats, and can return results collected.
//#####################################################################

#ifndef WINDOW_STAT_TRACKER
#define WINDOW_STAT_TRACKER

#include <vector>
#include <iostream>
#include <string>

// TODO: change and tune
#define DEFAULT_W (100)

// Epsilon values for write and ready-heavy workloads
#define WRITE_HEAVY_E (0.4) // smaller epsilon = larger message buffer
#define READ_HEAVY_E (0.935) // bigger epsilon = shallower tree


class window_stat_tracker {

private:

	// max size of the window
	int W;

	// A vector to store 1s (write) and 0s (read) to track
	// the recent W operations on the B^E Tree
	std::vector<int> window;
	
	// Eraseses the oldest value in the sliding window
	void erase_oldest() {
		window.erase(window.begin());
	}

	bool window_full() {
		int size = static_cast<int>(window.size());
		return (size >= W);
	}

	
public:
	// Constructor
	window_stat_tracker(int w = DEFAULT_W) : W(w)
       	{
	}
	
	
	// Method to get an epsilon value based off of
	// the sliding window of statistics
	float get_epsilon() {
		int write_count = get_write_count();

		// the percentage of writes in the window
		float write_percentage = write_count/W;

		float heavy_E_diff = READ_HEAVY_E - WRITE_HEAVY_E;

		// Normalizes the percentage from [0,1] to [WRITE_HEAVY_E,READ_HEAVY_E]
		float epsilon_in_range = READ_HEAVY_E - (heavy_E_diff * write_percentage);
		
		return epsilon_in_range;
	}


	// methods to update the window with recent operations
	void add_read() {
		// If window is full, remove oldest value
		if (window_full()) {
			erase_oldest();
		}
		window.push_back(0); // add a read event (0)
	}

	void add_write() {
		 // If window is full, remove oldest value
                if (window_full()) {
                        erase_oldest();
                }
		window.push_back(1); // add a write event (1)
	}

	// Sums the values in the window (0s (reads) and 1s (writes))
	// returns the sum, which is the number of writes in the window
	int get_write_count() {
		int write_count = 0;
		for (int val : window) {
			write_count += val;
		}
		return write_count;
	}

	// Loops over the window and counts the 0s (the read events)
	int get_read_count() {
		int read_count = 0;
		for (int val : window) {
			if (val == 0) {
				read_count += 1;
			}
		}
		return read_count;
	}

	// prints the read and write counts and the size of the window
	void print_read_write_count() {
		int writes = get_write_count();
		int reads = get_read_count();
		std::cout << "Write count is: " << std::to_string(writes) << std::endl;
		std::cout << "Read count is: " << std::to_string(reads) << std::endl;
		int total = writes + reads;
		std::cout << "Total in window: " << std::to_string(total) << std::endl;
	}
};

#endif
