#include "RtcDS3231.h"


void setup();
void loop();

void serial_routine();
void wake_routine();
void generate_report(const RtcDateTime&);

bool load_configuration();
bool update_rtc_time();