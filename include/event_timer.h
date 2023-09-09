#ifndef LIBQ_EVENT_TIMER
#define LIBQ_EVENT_TIMER
#pragma once

#include <thread>
#include <chrono>
#include <vector>
#include <cstdint>

// Creates a vector of etimer_child objects and 

namespace libQ {

class etimer_child
{
public:
	virtual void etimer_update() = 0;
	std::chrono::system_clock::time_point next_event;
};

class event_timer
{
public:
	event_timer();

	void registerNewChild(etimer_child *child);

	void update_children();

private:
	std::vector<etimer_child*> _timers;

	std::chrono::system_clock::time_point _start;
	std::chrono::system_clock::time_point _currentCycleTime;
};

};
#endif