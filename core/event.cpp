/**
 *
 * File: event.cpp
 * Author: Yarygin Alexander <zpp0@mail.ru>
 *
 **/

#include "event.h"

#include "env.h"

#include "simulator.h"

// quint64 Event::count = 0;

// Event::event()
// {
//     count++;
//     recordable = false;
// }

void EventFactory::post(IModule* author, QString name, VirtualTime time, QVariantList params)
{
    // TODO: bufferization
    Event* event = new Event();
    event->name = name;
    event->author = author;
    event->time = Env::time + time;
    if (time == 0)
        event->recordable = false;
    else
        event->recordable = true;
    event->params = params;

    Simulator::postEvent(author, event);
};
