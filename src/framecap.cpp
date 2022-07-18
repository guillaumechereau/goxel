/*
	A Simple Yet Weird Way Of Frame Capping
*/

#include <chrono>
#include <thread>

#include "framecap.h"

std::chrono::milliseconds wait_time;
std::chrono::time_point<std::chrono::steady_clock> start_time;
std::chrono::time_point next_time = start_time + wait_time;

void framecap_init(void) {
	wait_time = std::chrono::milliseconds{ 17 };
	start_time = std::chrono::steady_clock::now();
}

void framecap_sleep(void) {
	std::this_thread::sleep_until(next_time);
	next_time += wait_time;
}
