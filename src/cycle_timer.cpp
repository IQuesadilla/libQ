#include <iostream>
#include "cycle_timer.h"

namespace libQ {

cycle_timer::cycle_timer(uint cycleDuration)
{
	_cycleDuration = cycleDuration;
	_start = std::chrono::system_clock::now();
	_currentCycleTime = _start;
	_currentCycle = 0;
}

int cycle_timer::timestamp()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()
			- _start.time_since_epoch()
		).count();
}

int cycle_timer::cycleCount()
{
	return _currentCycle;
}

void cycle_timer::new_cycle()
{
	auto now = std::chrono::system_clock::now().time_since_epoch();

	auto delta = std::chrono::floor<std::chrono::milliseconds>(now - _start.time_since_epoch());
	_currentCycle = uint(delta.count() / _cycleDuration) + 1;

	auto msToNextCycle = std::chrono::milliseconds(_currentCycle * _cycleDuration);
	_currentCycleTime = _start + msToNextCycle;

	std::this_thread::sleep_until(_currentCycleTime);
}

void cycle_timer::delay_until(uint numberator, uint denominator)
{
	std::this_thread::sleep_until(
		_currentCycleTime
		+ (
			std::chrono::milliseconds(_cycleDuration / denominator)
			* numberator
		)
	);
}

};