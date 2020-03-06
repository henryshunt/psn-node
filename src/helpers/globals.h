#include <stdint.h>
#include <esp_attr.h>
#include <Wire.h>
#include <RtcDS3231.h>


#ifndef GLOBALS_H
#define GLOBALS_H

extern RTC_DATA_ATTR char mac_address[18];

extern RTC_DATA_ATTR char network_name[32];
extern RTC_DATA_ATTR bool is_enterprise_network;
extern RTC_DATA_ATTR char network_username[64];
extern RTC_DATA_ATTR char network_password[64];
extern RTC_DATA_ATTR char logger_address[32];
extern RTC_DATA_ATTR uint16_t logger_port;
extern RTC_DATA_ATTR uint8_t network_timeout;
extern RTC_DATA_ATTR uint8_t logger_timeout;

extern RtcDS3231<TwoWire> rtc;


bool load_configuration(bool*);
#endif