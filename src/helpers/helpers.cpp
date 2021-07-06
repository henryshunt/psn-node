#include "helpers.h"

/**
 * Rounds a number up to a multiple of another number.
 * @param number The number to round.
 * @param multiple The number will be rounded up to a multiple of this number.
 * @returns The rounded number.
 */
int roundUpMultiple(int number, int multiple)
{
    if (multiple == 0)
        return number;

    int remainder = number % multiple;

    if (remainder == 0)
        return number;

    return number + multiple - remainder;
}

/**
 * Formats a time into a string of the format yyyy-MM-ddTHH:mm:ss.
 * @param time The time to serialise.
 * @param timeOut The destination for the serialised time.
*/
void formatTime(const RtcDateTime& time, char* const timeOut)
{
    sprintf(timeOut, "%04u-%02u-%02uT%02u:%02u:%02u", time.Year(),
        time.Month(), time.Day(), time.Hour(), time.Minute(), time.Second());
}