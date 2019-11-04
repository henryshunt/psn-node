#include "RtcDS3231.h"


void setup();
void loop();

void alarm_triggered();

void generate_report(const RtcDateTime&);

bool network_connect();
bool broker_connect();

void set_rtc_time();