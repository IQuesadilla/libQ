#include "cycle_timer.h"
#include <iostream>

int main()
{
	libQ::cycle_timer mytimer(100);
	for (int i = 0; i < 10; ++i)
	{
		mytimer.new_cycle();
		std::cout << mytimer.timestamp() << " ID: " << mytimer.cycleCount() << " - New cycle" << std::endl;

		mytimer.delay_until(1,3);
		std::cout << mytimer.timestamp() << " ID: " << mytimer.cycleCount() << " - First third" << std::endl;

		mytimer.delay_until(2,3);
		std::cout << mytimer.timestamp() << " ID: " << mytimer.cycleCount() << " - Second third" << std::endl;
	}
	
	return 0;
}