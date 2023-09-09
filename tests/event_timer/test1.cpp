#include "event_timer.h"

#include <iostream>

class my_etimer_child : public libQ::etimer_child
{
public:
    void etimer_update()
    {
        std::cout << "function: etimer_update" << std::endl;
        myUpdate();
    }

private:
    void myUpdate()
    {
        std::cout << std::chrono::system_clock::now().time_since_epoch().count() << " : myUpdate" << std::endl;
        next_event = std::chrono::system_clock::now() + std::chrono::milliseconds(1000);
    }
};

int main()
{
    libQ::event_timer timer;

    my_etimer_child object;
    timer.registerNewChild(&object);

    for (int i = 0; i < 5; ++i)
    {
        timer.update_children();
    }
}