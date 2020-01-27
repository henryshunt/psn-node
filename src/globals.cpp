#include <Preferences.h>

#include "RtcDS3231.h"
#include "AsyncMqttClient.h"

#include "globals.h"
#include "helpers/helpers.h"
#include "helpers/buffer.h"


// Public configuration
RTC_DATA_ATTR bool NETWORK_ENTERPRISE;
RTC_DATA_ATTR char NETWORK_NAME[32];
RTC_DATA_ATTR char NETWORK_USERNAME[64];
RTC_DATA_ATTR char NETWORK_PASSWORD[64];
RTC_DATA_ATTR char LOGGER_ADDRESS[32];
RTC_DATA_ATTR uint16_t LOGGER_PORT;
RTC_DATA_ATTR uint8_t NETWORK_TIMEOUT;
RTC_DATA_ATTR uint8_t LOGGER_TIMEOUT;

// Private configuration
#define SERIAL_TIMEOUT 5
#define BUFFER_MAX 10
#define ALARM_SET_THRESHOLD 5
#define RTC_SQUARE_WAVE_PIN GPIO_NUM_35

// Network related
bool awaiting_subscribe = false;
uint16_t subscribe_id = -1;
bool awaiting_session = false;
bool awaiting_report = false;
ReportResult report_result = ReportResult::None;
uint16_t publish_id = -1;

// Persisted between deep sleeps
RTC_DATA_ATTR bool cold_boot = true;
RTC_DATA_ATTR char mac_address[18] = { '\0' };
RTC_DATA_ATTR int session_id = -1;
RTC_DATA_ATTR int session_interval = -1;
RTC_DATA_ATTR int session_batch_size = -1;
RTC_DATA_ATTR report_buffer_t buffer;
RTC_DATA_ATTR report_t reports[BUFFER_MAX];

// Services
Preferences config;
RtcDS3231<TwoWire> rtc(Wire);
AsyncMqttClient logger;