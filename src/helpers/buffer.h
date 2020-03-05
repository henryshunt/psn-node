#include "RtcDS3231.h"

#include "helpers.h"


#ifndef BUFFER_H
#define BUFFER_H

/*
    Limited implementation of a circular buffer, designed for use in the ESP32's sleep
    memory. Elements are added to the front and removed fron the rear. Front points to
    the index that the next added element will occupy, while rear points to the index
    that currently holds the final element.

    Before usage, prepare an array of size BUFFER_CAPACITY to store the elements in.
 */
struct report_buffer_t
{
private:    
    int front = 0;
    int rear = 0;

public:
    const int count()
    {
        if (front == rear)
            return 0;
        else if (rear < front)
            return front - rear;
        else return ((BUFFER_CAPACITY + 1) - rear) + front;
    }

    const bool is_empty()
    {
        return count() == 0 ? true : false;
    }

    const bool is_full()
    {
        return count() == (BUFFER_CAPACITY + 1) - 1 ? true : false;
    }


    void push_front(report_t* elements, const report_t& report)
    {
        elements[front].time = report.time;
        elements[front].airt = report.airt;
        elements[front].relh = report.relh;
        elements[front].batv = report.batv;

        bool full = is_full();
        front = (front + 1) % (BUFFER_CAPACITY + 1);

        if (full)
            rear = (rear + 1) % (BUFFER_CAPACITY + 1);
    }

    report_t pop_rear(report_t* elements)
    {
        report_t report = elements[rear];
        rear = (rear + 1) % (BUFFER_CAPACITY + 1);
        return report;
    }

    const report_t peek_rear(report_t* elements)
    {
        return elements[rear];
    }
};

#endif