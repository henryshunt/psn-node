#include <Wire.h>

#include "RtcDS3231.h"

#include "buffer.h"


void print_time(const RtcDateTime&);
int round_up(int, int);

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

RtcDateTime get_next_alarm(const RtcDateTime&, int);
void set_alarm(RtcDS3231<TwoWire>&, const RtcDateTime&);
void report_to_string(char*, const report_t&, char*);