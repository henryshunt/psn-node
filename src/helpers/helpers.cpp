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
    if (!rtc.LastError())
    {
        if (!rtc_valid) return false;
    } else return false;

    return true;
}

/*
    Sets the RTC alarm to the specified time
 */
void set_alarm(RtcDS3231<TwoWire>& rtc, const RtcDateTime& time)
{
    DS3231AlarmOne alarm(
        time.Day(), time.Hour(), time.Minute(), time.Second(),
        DS3231AlarmOneControl_MinutesSecondsMatch);
    rtc.SetAlarmOne(alarm);
    rtc.LatchAlarmsTriggeredFlags();
}

/*
    Serialises the supplied report struct into a JSON string
 */
void report_to_string(char* report_out, const report_t& report, int session)
{
    int length = 0;
    length += sprintf(report_out, "{ \"session\": %d", session);

    char formatted_time[32] = { '\0' };
    format_time(formatted_time, RtcDateTime(report.time));
    length += sprintf(
        report_out + length, ", \"time\": \"%s\"", formatted_time);

    if (report.airt != -99)
        length += sprintf(report_out + length, ", \"airt\": %.2f", report.airt);
    else length += sprintf(report_out + length, ", \"airt\": null");

    if (report.relh != -99)
        length += sprintf(report_out + length, ", \"relh\": %.2f", report.relh);
    else length += sprintf(report_out + length, ", \"relh\": null");

    if (report.batv != -99)
        length += sprintf(report_out + length, ", \"batv\": %.2f", report.batv);
    else length += sprintf(report_out + length, ", \"batv\": null");

    strcat(report_out + length, " }");
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
    Converts a timestamp into YYYY-MM-DDTHH:mm:SS format
*/
void format_time(char* time_out, const RtcDateTime& time)
{
    sprintf(time_out, "%04u-%02u-%02uT%02u:%02u:%02uZ", time.Year(),
        time.Month(), time.Day(), time.Hour(), time.Minute(), time.Second());
}