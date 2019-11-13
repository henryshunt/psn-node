#include "RtcDS3231.h"

void setup();
void loop();

void wake_routine();
void generate_report(const RtcDateTime&);

bool serial_connect();
bool network_connect();
bool logger_connect();
bool logger_subscribe();
bool logger_session();
bool logger_report(const char*, uint32_t);

void logger_on_subscribe(uint16_t, uint8_t);
void logger_on_message(char*, char*, AsyncMqttClientMessageProperties, size_t,
    size_t, size_t);

bool update_rtc_time();