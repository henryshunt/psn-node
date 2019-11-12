#include "RtcDS3231.h"

void setup();
void loop();

void wake_routine();
void generate_report(const RtcDateTime&);

bool network_connect();
bool broker_connect();
bool broker_subscribe();
bool broker_session();
bool broker_report(const char*, uint32_t);

void broker_on_subscribe(uint16_t, uint8_t);
void broker_on_message(char*, char*, AsyncMqttClientMessageProperties, size_t,
    size_t, size_t);

void set_rtc_time();