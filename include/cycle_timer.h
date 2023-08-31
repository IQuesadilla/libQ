#ifndef LIBQ_CYCLE_TIMER
#define LIBQ_CYCLE_TIMER
#pragma once

#include <thread>
#include <chrono>

namespace libQ {

typedef unsigned int uint;

class cycle_timer
{
public:
	cycle_timer(uint cycleDuration);

	int timestamp();
	int cycleCount();
	void new_cycle();
	void delay_until(uint numberator, uint denominator);

private:
	std::chrono::system_clock::time_point _start;
	uint _currentCycle;
	std::chrono::system_clock::time_point _currentCycleTime;

	uint _cycleDuration;
};

};
#endif