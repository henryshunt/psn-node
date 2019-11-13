#include <Wire.h>

#include "RtcDS3231.h"

#include "helpers.h"
#include "buffer.h"


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
void report_to_string(char* report_out, const report_t& report,
    int session_id, char* mac_address)
{
    sprintf(report_out, "{ \"session\": %d", session_id);

    if (report.airt != -99)
        concat_value<float>(report_out, ", \"airt\": %.1f", report.airt);
    else strcat(report_out, ", \"airt\": null");

    if (report.relh != -99)
        concat_value<float>(report_out, ", \"relh\": %.1f", report.relh);
    else strcat(report_out, ", \"relh\": null");

    if (report.lvis != -99)
        concat_value<float>(report_out, ", \"lvis\": %.1f", report.lvis);
    else strcat(report_out, ", \"lvis\": null");

    if (report.lifr != -99)
        concat_value<float>(report_out, ", \"lifr\": %.1f", report.lifr);
    else strcat(report_out, ", \"lifr\": null");

    if (report.batv != -99)
        concat_value<float>(report_out, ", \"batv\": %.1f", report.batv);
    else strcat(report_out, ", \"batv\": null");

    strcat(report_out, " }");
}