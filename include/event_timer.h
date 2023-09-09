#ifndef LIBQ_EVENT_TIMER
#define LIBQ_EVENT_TIMER
#pragma once

#include <thread>
#include <chrono>
#include <vector>
#include <cstdint>
#include <memory>
#include <functional>

// Creates a vector of etimer_child objects and 

namespace libQ {

class etimer_child
{
public:
	virtual ~etimer_child(){};
	virtual void etimer_update() = 0;
	std::chrono::system_clock::time_point next_event;
};

template <typename T>
class event_timer
{
public:
	event_timer(std::shared_ptr<std::vector<T> > ptr)
	{
		_ptr = ptr;
	}

	void update_children()
	{
		T soonest = _ptr->at(0);
		for (auto &child : *_ptr)
		{
			if ( child->next_event < soonest->next_event )
			{
				soonest = child;
			}
		}

		std::this_thread::sleep_until(soonest->next_event);
		soonest->etimer_update();
	}

private:
	std::shared_ptr<std::vector<T> > _ptr;
};

};
#endif