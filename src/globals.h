#include <Preferences.h>

#include "RtcDS3231.h"
#include "AsyncMqttClient.h"

#include "helpers/helpers.h"
#include "helpers/buffer.h"


#ifndef GLOBALS_H
#define GLOBALS_H

// Public configuration
extern RTC_DATA_ATTR bool NETWORK_ENTERPRISE;
extern RTC_DATA_ATTR char NETWORK_NAME[32];
extern RTC_DATA_ATTR char NETWORK_USERNAME[64];
extern RTC_DATA_ATTR char NETWORK_PASSWORD[64];
extern RTC_DATA_ATTR char LOGGER_ADDRESS[32];
extern RTC_DATA_ATTR uint16_t LOGGER_PORT;
extern RTC_DATA_ATTR uint8_t NETWORK_TIMEOUT;
extern RTC_DATA_ATTR uint8_t LOGGER_TIMEOUT;

// Private configuration
#define SERIAL_TIMEOUT 5
#define BUFFER_MAX 10
#define ALARM_SET_THRESHOLD 5
#define RTC_SQUARE_WAVE_PIN GPIO_NUM_35

// Network related
extern bool awaiting_subscribe;
extern uint16_t subscribe_id;
extern bool awaiting_session;
extern bool awaiting_report;
extern ReportResult report_result;
extern uint16_t publish_id;

// Persisted between deep sleeps
extern RTC_DATA_ATTR bool cold_boot;
extern RTC_DATA_ATTR char mac_address[18];
extern RTC_DATA_ATTR int session_id;
extern RTC_DATA_ATTR int session_interval;
extern RTC_DATA_ATTR int session_batch_size;
extern RTC_DATA_ATTR report_buffer_t buffer;
extern RTC_DATA_ATTR report_t reports[BUFFER_MAX];

// Services
extern Preferences config;
extern RtcDS3231<TwoWire> rtc;
extern AsyncMqttClient logger;

#endif