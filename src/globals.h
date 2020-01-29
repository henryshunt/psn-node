#include <stdint.h>
#include <esp_attr.h>


#ifndef GLOBALS_H
#define GLOBALS_H

extern RTC_DATA_ATTR char mac_address[18];

extern RTC_DATA_ATTR bool NETWORK_ENTERPRISE;
extern RTC_DATA_ATTR char NETWORK_NAME[32];
extern RTC_DATA_ATTR char NETWORK_USERNAME[64];
extern RTC_DATA_ATTR char NETWORK_PASSWORD[64];
extern RTC_DATA_ATTR char LOGGER_ADDRESS[32];
extern RTC_DATA_ATTR uint16_t LOGGER_PORT;
extern RTC_DATA_ATTR uint8_t NETWORK_TIMEOUT;
extern RTC_DATA_ATTR uint8_t LOGGER_TIMEOUT;


void load_mac_address();
bool load_configuration(bool*);
#endif