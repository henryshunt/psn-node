#include <Wire.h>

#include "RtcDS3231.h"

#include "helpers.h"


/*
    Prints the supplied time in a friendly format
*/
void print_time(const RtcDateTime& time)
{
    char _time[32] = { '\0' };
    sprintf(_time, "%04u-%02u-%02u %02u:%02u:%02u", time.Year(),
        time.Month(), time.Day(), time.Hour(),
        time.Minute(), time.Second());
    Serial.println(_time);
}

/*
    Advances a number up to the next multiple of another number
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
    Sets the RTC alarm to the current time plus the interval
*/
void set_next_alarm(RtcDS3231<TwoWire>& rtc,
    const RtcDateTime& now, int interval)
{
    RtcDateTime alarmTime = now + (interval * 60);
    DS3231AlarmOne alarm(
        alarmTime.Day(), alarmTime.Hour(), alarmTime.Minute(),
        alarmTime.Second(), DS3231AlarmOneControl_MinutesSecondsMatch);
    rtc.SetAlarmOne(alarm);
    rtc.LatchAlarmsTriggeredFlags();
}