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
    {
        std::vector<my_etimer_child*> objarray;
        std::shared_ptr<std::vector<my_etimer_child*>> objects(&objarray,[](void*){});

        my_etimer_child object1(1000);
        objects->push_back(&object1);
        //timer.push_back(new my_etimer_child(&object1));

        my_etimer_child object2(300);
        objects->push_back(&object2);
        //timer.push_back(std::shared_ptr<my_etimer_child>(&object2,[](void*){}));

        my_etimer_child object3(1300);
        objects->push_back(&object3);
        //timer.push_back(std::shared_ptr<my_etimer_child>(&object3,[](void*){}));

        libQ::event_timer<my_etimer_child*> timer(objects);

        for (int i = 0; i < 15; ++i)
        {
            timer.update_children();
        }
    }

    {
        std::shared_ptr<std::vector<my_etimer_child*>> objects;
        objects.reset(new std::vector<my_etimer_child*>);
        libQ::event_timer<my_etimer_child*> timer(objects);
    
        objects->push_back(new my_etimer_child(1000));
        objects->push_back(new my_etimer_child(300));
        objects->push_back(new my_etimer_child(1300));

        for (int i = 0; i < 15; ++i)
        {
            timer.update_children();
        }

        for (auto &x : *objects)
        {
            delete x;
        }
    }
}