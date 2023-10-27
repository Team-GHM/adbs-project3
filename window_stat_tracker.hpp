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
#include <cmath>

// TODO: change and tune
#define DEFAULT_W (100)

class window_stat_tracker {

private:

	// max size of the window
	int W;

	// A vector to store 1s (write) and 0s (read) to track
	// the recent W operations on the B^E Tree
	std::vector<int> window;
	
	// Eraseses the oldest value in the sliding window
	void eraseOldest() {
		window.erase(window.begin());
	}

	bool windowFull() {
		int size = static_cast<int>(window.size());
		return (size >= W);
	}

	
public:
	// Constructor
	window_stat_tracker(int w = DEFAULT_W) : W(w)
       	{
		std::cout << "initing window ..." << std::endl;
	}
	

	// methods to update the window with recent operations
	void add_read() {
		std::cout << "adding read ..." << std::endl;

		// If window is full, remove oldest value
		if (windowFull()) {
			eraseOldest();
		}

		window.push_back(0); // add a read event (0)
		print_read_write_count();
	}

	void add_write() {
		std::cout << "adding write ..." << std::endl;
		 // If window is full, remove oldest value
                if (windowFull()) {
                        eraseOldest();
                }

		window.push_back(1); // add a write event (1)
		print_read_write_count();
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

	// Gets the write count and subtracts that from W 
	// which is the read count
	int get_read_count() {
		int write_count = get_write_count();
		int read_count = W - write_count;
		return read_count;
	}

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
