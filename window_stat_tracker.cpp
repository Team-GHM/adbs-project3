#include "window_stat_tracker.hpp"


window_stat_tracker::window_stat_tracker(int w) : W(w)
{
}

void window_stat_tracker::add_read(){
	std::cout << "Adding read ..." << std::endl;
}

void window_stat_tracker::add_write(){
	std::cout << "Adding write ..." << std::endl;
}
