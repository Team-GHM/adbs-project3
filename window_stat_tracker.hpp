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
	
public:
	// Constructor
	window_stat_tracker(int w = DEFAULT_W);
	
	// methods to update the window with recent operations
	void add_read();
	void add_write();
};

#endif
