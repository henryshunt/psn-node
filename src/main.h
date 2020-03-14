#include <RtcDS3231.h>

#include "helpers/helpers.h"


void setup();
void try_serial_mode();
bool connect_and_get_session();
void set_first_alarm();
void loop();

void reporting_routine();
void generate_report(const RtcDateTime&);
void serialise_report(char*, const report_t&);


bool is_rtc_time_valid();
void set_rtc_alarm(const RtcDateTime&);