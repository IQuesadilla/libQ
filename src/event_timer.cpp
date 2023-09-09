#include <iostream>
#include "event_timer.h"

namespace libQ {

event_timer::event_timer()
{
	_start = std::chrono::system_clock::now();
}

void event_timer::registerNewChild(etimer_child *child)
{
	_timers.push_back(child);
}

void event_timer::update_children()
{
	etimer_child *soonest = _timers[0];
	for (auto &child : _timers)
	{
		if ( child->next_event < soonest->next_event )
		{
			soonest = child;
		}
	}

	std::this_thread::sleep_until(soonest->next_event);
	soonest->etimer_update();
}

};