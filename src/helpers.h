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
    int lvis;
    int lifr;
    float batv;
};

/*
    Concatenates a parameter and value onto the report
 */
template<typename T>
void concat_value(char* report, const char* value_string, T value)
{
    char section[32] = { '\0' };
    sprintf(section, value_string, value);
    strcat(report, section);
}

int round_up(int, int);
void set_alarm(RtcDS3231<TwoWire>&, const RtcDateTime&);
void report_to_string(char*, const report_t&, int, char*);

#endif