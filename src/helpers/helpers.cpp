/*
    Various helper functions for use throughout the codebase. See helpers.h for
    definitions of various structs and global configurable values.
 */

#include <RtcDS3231.h>

#include "helpers.h"
#include "buffer.h"


/*
    Rounds a number up to a multiple of another number.
    Taken from https://stackoverflow.com/questions/3407012/c-rounding-up-to-the-nearest-multiple-of-a-number

    - number: the number to round
    - multiple: a number, the multiple of which to round up to
 */
int round_up_multiple(int number, int multiple)
{
    if (multiple == 0) return number;

    int remainder = number % multiple;
    if (remainder == 0) return number;

    return number + multiple - remainder;
}

/*
    Serialises a time into an ISO 8601 formatted string.

    - time_out: destination string
    - time: the time to serialise
*/
void format_time(char* time_out, const RtcDateTime& time)
{
    sprintf(time_out, "%04u-%02u-%02uT%02u:%02u:%02uZ", time.Year(),
        time.Month(), time.Day(), time.Hour(), time.Minute(), time.Second());
}