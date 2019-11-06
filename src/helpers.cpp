#include <Wire.h>

#include "RtcDS3231.h"

#include "helpers.h"
#include "buffer.h"


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
    Calculates the next alarm time based on the required interval
 */
RtcDateTime get_next_alarm(const RtcDateTime& time, int interval)
{
    // * 60 converts minutes (interval) to seconds (time)
    RtcDateTime next_alarm = round_up(time, interval * 60);

    if (next_alarm == time)
        return round_up(time + 1, interval * 60);
    else return next_alarm;
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
void report_to_string(char* report_out, const report_t& report, char* mac_address)
{
    // Format the report timestamp
    RtcDateTime time = RtcDateTime(report.time);
    char time_out[20] = { '\0' };

    sprintf(time_out, "%04u-%02u-%02u %02u:%02u:%02u", time.Year(),
        time.Month(), time.Day(), time.Hour(), time.Minute(), time.Second());

    // Generate the report
    sprintf(report_out, "{ \"node\": \"%s\", \"time\": \"%s\"", mac_address,
        time_out);

    Serial.println(report.airt);
    Serial.println(report.airt_ok);

    // if (report.airt_ok)
    concat_value<float>(report_out, ", \"airt\": %.1f", report.airt);
    // else strcat(report_out, ", \"airt\": null");

    // if (report.relh_ok)
    concat_value<float>(report_out, ", \"relh\": %.1f", report.relh);
    // else strcat(report_out, ", \"relh\": null");

    if (report.lvis_ok)
        concat_value<float>(report_out, ", \"lvis\": %.1f", report.lvis);
    else strcat(report_out, ", \"lvis\": null");

    if (report.lifr_ok)
        concat_value<float>(report_out, ", \"lifr\": %.1f", report.lifr);
    else strcat(report_out, ", \"lifr\": null");

    if (report.batv_ok)
        concat_value<float>(report_out, ", \"batv\": %.1f", report.batv);
    else strcat(report_out, ", \"batv\": null");

    strcat(report_out, " }");
}