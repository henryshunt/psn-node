#include <Wire.h>

#include "RtcDS3231.h"


#ifndef HELPERS_H
#define HELPERS_H

enum ReportResult { None, Ok, NoSession };
struct report_t
{
    uint32_t time;
    float airt;
    float relh;
    long lght;
    float batv;
};

bool rtc_time_valid(RtcDS3231<TwoWire>&);
void set_alarm(RtcDS3231<TwoWire>&, const RtcDateTime&);
void report_to_string(char*, const report_t&, int);
int round_up(int, int);
void format_time(char*, const RtcDateTime&);

#endif