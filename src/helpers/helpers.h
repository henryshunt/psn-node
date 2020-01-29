#include <Wire.h>

#include "RtcDS3231.h"


#ifndef HELPERS_H
#define HELPERS_H

struct session_t
{
    int32_t session;
    int8_t interval;
    int8_t batch_size;
};

enum RequestResult { Success, Fail, NoSession };
struct report_t
{
    uint32_t time;
    float airt;
    float relh;
    float batv;
};

bool is_rtc_time_valid(RtcDS3231<TwoWire>&);
void set_alarm(RtcDS3231<TwoWire>&, const RtcDateTime&);
void report_to_string(char*, const report_t&, int);
int round_up(int, int);
void format_time(char*, const RtcDateTime&);
#endif