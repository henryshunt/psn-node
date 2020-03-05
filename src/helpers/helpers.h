#include <Wire.h>

#include "RtcDS3231.h"


#ifndef HELPERS_H
#define HELPERS_H

enum RequestResult { Success, Fail, NoSession };

struct session_t
{
    int32_t session_id;
    int8_t interval;
    int8_t batch_size;
};

struct report_t
{
    uint32_t time;
    float airt;
    float relh;
    float batv;
};


bool is_rtc_time_valid(RtcDS3231<TwoWire>&);
void set_rtc_alarm(RtcDS3231<TwoWire>&, const RtcDateTime&);
int round_up(int, int);
void format_time(char*, const RtcDateTime&);
#endif