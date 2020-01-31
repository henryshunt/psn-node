#include <Wire.h>

#include "RtcDS3231.h"

#include "helpers.h"
#include "buffer.h"


/*
    Checks if the RTC is holding a valid timestamp
*/
bool is_rtc_time_valid(RtcDS3231<TwoWire>& rtc)
{
    bool rtc_valid = rtc.IsDateTimeValid();
    return rtc.LastError() ? false : rtc_valid;
}

/*
    Sets the RTC alarm to the specified time
 */
void set_rtc_alarm(RtcDS3231<TwoWire>& rtc, const RtcDateTime& time)
{
    DS3231AlarmOne alarm(time.Day(), time.Hour(), time.Minute(), time.Second(),
        DS3231AlarmOneControl_MinutesSecondsMatch);

    rtc.SetAlarmOne(alarm);
    rtc.LatchAlarmsTriggeredFlags();
}

/*
    Rounds a number up to a multiple of another number
    Taken from https://stackoverflow.com/questions/3407012/c-rounding-up-to-the-nearest-multiple-of-a-number
    Lightly refactored (name changes and formatting)
 */
int round_up(int number, int multiple)
{
    if (multiple == 0) return number;

    int remainder = number % multiple;
    if (remainder == 0) return number;

    return number + multiple - remainder;
}

/*
    Serialises the supplied timestamp into an ISO 8601 formatted string
*/
void format_time(char* time_out, const RtcDateTime& time)
{
    sprintf(time_out, "%04u-%02u-%02uT%02u:%02u:%02uZ", time.Year(),
        time.Month(), time.Day(), time.Hour(), time.Minute(), time.Second());
}