#include "event_timer.h"

#include <iostream>

class my_etimer_child : public libQ::etimer_child
{
public:
    my_etimer_child(int msdelay) : libQ::etimer_child()
    {
        _msdelay = std::chrono::milliseconds(msdelay);
        _start = std::chrono::system_clock::now();
        next_event = _start + _msdelay;
    }

    void etimer_update()
    {
        myUpdate();
    }

private:
    void myUpdate()
    {
        std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _start).count() << " : " << _msdelay.count() << " : myUpdate" << std::endl;
        next_event += _msdelay;
    }

    std::chrono::milliseconds _msdelay;
    std::chrono::system_clock::time_point _start;
};

int main()
{
    libQ::event_timer timer;

    my_etimer_child object1(1000);
    timer.registerNewChild(&object1);

    my_etimer_child object2(300);
    timer.registerNewChild(&object2);

    my_etimer_child object3(1300);
    timer.registerNewChild(&object3);

    for (int i = 0; i < 15; ++i)
    {
        timer.update_children();
    }
}