#include "RtcDS3231.h"


void setup();
void loop();

void rtc_alarm_triggered();

void produce_report(const RtcDateTime&);

bool network_connect();
bool broker_connect();

void set_rtc_time();