#ifndef BUFFER_H
#define BUFFER_H

#include "utilities.h"

/**
 * A limited implementation of a circular buffer, designed specifically for storage in the
 * ESP32's sleep memory. Elements are added to the front and removed from the rear.
 * 
 * Before usage, prepare an array of size BUFFER_CAPACITY to store the elements in. This 
 * must be passed into any functions that operate on the elements array. Normally the array
 * would be a member of the class, but the buffer is stored in the sleep memory which has
 * limited abilities and so the array must be kept separate in order for it to work.
 */
class buffer_t
{
    /**
     * The front of the buffer (the index that the next element to be pushed will occupy).
     */
    int front = 0;

    /**
     * The rear of the buffer (the index that currently holds the final element).
     */
    int rear = 0;

public:
    /**
     * Returns the number of elements in the buffer.
     */
    const int count();

    /**
     * Returns whether the buffer is empty.
     */
    const bool isEmpty();

    /**
     * Returns whether the buffer is full.
     */
    const bool isFull();

    /**
     * Pushes an observation onto the front of the buffer.
     * @param elements The elements of the buffer.
     * @param obs The observation to push onto the buffer.
     */
    void pushFront(observation_t* const elements, const observation_t& obs);

    /**
     * Removes and returns the observation at the rear of the buffer.
     * @param elements The elements of the buffer.
     * @returns The popped observation, or a null pointer if there is no observation to
     * pop.
     */
    observation_t* popRear(observation_t* const elements);

    /**
     * Returns the element at the rear of the buffer.
     * @param elements The elements of the buffer.
     * @returns The peeked observation, or a null pointer if there is no observation to
     * peek.
     */
    observation_t* peekRear(observation_t* const elements);
};

#endif