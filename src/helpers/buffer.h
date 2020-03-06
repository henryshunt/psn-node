/*
    A limited implementation of a circular buffer, designed specifically for
    storage in the ESP32's sleep memory. Elements are added to the front and
    removed fron the rear.

    Before usage, prepare an array of size BUFFER_CAPACITY to store the elements
    in. This must be passed into any functions that require the elements.
 */

#include "helpers.h"

#ifndef BUFFER_H
#define BUFFER_H

struct report_buffer_t
{
private:
    /*
        Points to the front of the buffer (the index that the next element to be
        pushed will occupy).
     */
    int front = 0;

    /*
        Points to the rear of the buffer (the index that currently holds the
        final element).
     */
    int rear = 0;

public:
    /*
        Returns the number of elements in the buffer.
     */
    const int count()
    {
        if (front == rear)
            return 0;
        else if (rear < front)
            return front - rear;
        else return ((BUFFER_CAPACITY + 1) - rear) + front;
    }

    /*
        Returns a boolean indicating whether the buffer is empty or not.
     */
    const bool is_empty()
    {
        return count() == 0 ? true : false;
    }

    /*
        Returns a boolean indicating whether the buffer is full or not.
     */
    const bool is_full()
    {
        return count() == (BUFFER_CAPACITY + 1) - 1 ? true : false;
    }


    /*
        Pushes a report onto the front of the buffer.

        - elements: array containing the elements of the buffer
        - report: the report to push onto the buffer
     */
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

    /*
        Removes and returns the element at the rear of the buffer.

        - elements: array containing the elements of the buffer
     */
    report_t pop_rear(report_t* elements)
    {
        report_t report = elements[rear];
        rear = (rear + 1) % (BUFFER_CAPACITY + 1);
        return report;
    }

    /*
        Returns the element at the rear of the buffer.

        - elements: array containing the elements of the buffer
     */
    const report_t peek_rear(report_t* elements)
    {
        return elements[rear];
    }
};

#endif