#include "RtcDS3231.h"

#include "helpers.h"


#ifndef BUFFER_H
#define BUFFER_H

/*
    Limited implementation of a circular buffer, designed specifically for this
    use case (due to limitations posed by ESP32 sleep memory)
 */
struct report_buffer_t
{
private:
    int front = 0;
    int rear = 0;

    /*
        Determines whether the buffer can fit no more elements in
     */
    bool is_full()
    {
        return count == size ? true : false;
    }

public:
    int size = 1; // Should be const but has to be set externally
    int count = 0;

    /*
        Determines whether the buffer has no elements in
     */
    bool is_empty()
    {
        return count == 0 ? true : false;
    }

    /*
        Adds an element to the front of the buffer
     */
    void push_front(report_t* elements, const report_t& report)
    {
        elements[front].time = report.time;
        elements[front].airt = report.airt;
        elements[front].relh = report.relh;
        elements[front].batv = report.batv;
        front = (front + 1) % size;

        if (is_full())
            rear = (rear + 1) % size;
        else count += 1;
    }

    /*
        Adds an element to the rear of the buffer
     */
    void push_rear(report_t* elements, const report_t& report)
    {
        elements[rear].time = report.time;
        elements[rear].airt = report.airt;
        elements[rear].relh = report.relh;
        elements[rear].batv = report.batv;

        rear = (rear - 1) % size;
        count += 1;
    }

    /*
        Removes and returns an element from the rear of the buffer
     */
    report_t pop_rear(report_t* elements)
    {
        report_t report = elements[rear];
        rear = (rear + 1) % size;
        count -=1;
        return report;
    }
};

#endif