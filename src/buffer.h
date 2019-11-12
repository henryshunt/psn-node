#include "RtcDS3231.h"

#ifndef BUFFER_H
#define BUFFER_H

struct report_t
{
    uint32_t time;
    float airt = 0;
    bool airt_ok = false;
    float relh = 0;
    bool relh_ok = false;
    int lvis = 0;
    bool lvis_ok = false;
    int lifr = 0;
    bool lifr_ok = false;
    float batv = 0;
    bool batv_ok = false;
};

/*
    A limited implementation of a circular buffer, designed specifically for
    this use case
 */
struct report_buffer_t
{
private:
    int front = 0;
    int rear = 0;

    bool is_full()
    {
        return (count == MAX_SIZE) ? true : false;
    }

public:
    int MAX_SIZE = 1; // Should be const but must be set externally
    int count = 0;

    bool is_empty()
    {
        return (count == 0) ? true : false;
    }

    /*
        Adds an element to the front of the buffer
     */
    void push_front(report_t* elements, const report_t& report)
    {
        elements[front].time = report.time;
        elements[front].airt = report.airt;
        elements[front].relh = report.relh;
        elements[front].lvis = report.lvis;
        elements[front].lifr = report.lifr;
        elements[front].batv = report.batv;
        front = (front + 1) % MAX_SIZE;

        if (is_full())
            rear = (rear + 1) % MAX_SIZE;
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
        elements[rear].lvis = report.lvis;
        elements[rear].lifr = report.lifr;
        elements[rear].batv = report.batv;

        rear = (rear - 1) % MAX_SIZE;
        count += 1;
    }

    /*
        Removes and returns an element from the rear of the buffer
     */
    report_t pop_rear(report_t* elements)
    {
        report_t report = elements[rear];
        rear = (rear + 1) % MAX_SIZE;
        count -=1;
        return report;
    }
};

#endif