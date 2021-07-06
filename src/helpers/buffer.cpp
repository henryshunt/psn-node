#include "buffer.h"

const int buffer_t::count()
{
    if (front == rear)
        return 0;
    else if (rear < front)
        return front - rear;
    else return ((BUFFER_CAPACITY + 1) - rear) + front;
}

const bool buffer_t::isEmpty()
{
    return count() == 0 ? true : false;
}

const bool buffer_t::isFull()
{
    return count() == (BUFFER_CAPACITY + 1) - 1;
}

void buffer_t::pushFront(observation_t* const elements, const observation_t& obs)
{
    elements[front] = obs;

    bool full = isFull();
    front = (front + 1) % (BUFFER_CAPACITY + 1);

    if (full)
        rear = (rear + 1) % (BUFFER_CAPACITY + 1);
}

observation_t* buffer_t::popRear(observation_t* const elements)
{
    if (isEmpty())
        return NULL;

    observation_t* observation = &elements[rear];
    rear = (rear + 1) % (BUFFER_CAPACITY + 1);
    return observation;
}

observation_t* buffer_t::peekRear(observation_t* const elements)
{
    if (isEmpty())
        return NULL;
        
    return &elements[rear];
}