#include <Wire.h>

#include "RtcDS3231.h"


void print_time(const RtcDateTime& time);
int round_up(int number, int multiple);

/*
    Concatenates a parameter and value onto the report
*/
template<typename T>
void concat_report_value(char* report, const char* value_string, T value)
{
    char section[32] = { '\0' };
    sprintf(section, value_string, value);
    strcat(report, section);
}

void set_next_alarm(RtcDS3231<TwoWire>& rtc, const RtcDateTime& now,
    int interval);