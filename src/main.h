#include "RtcDS3231.h"


void setup();
void loop();
void wakeup_routine();

void generate_report(const RtcDateTime&);

bool connect_network();
bool connect_broker();
bool broker_publish(const char*, const char*);

void set_rtc_time();