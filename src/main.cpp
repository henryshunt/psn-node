#include <Wire.h>
#include <WiFiUdp.h>
#include <WiFi.h>

#include "RtcDS3231.h"
#include "NTPClient.h"
#include "AsyncMqttClient.h"
#include "Adafruit_Sensor.h"
#include "Adafruit_BME680.h"
#include "ArduinoJson.h"

#include "main.h"
#include "helpers.h"
#include "buffer.h"


// User configurable
const char* NETWORK_ID = "***REMOVED***";
const char* NETWORK_USERNAME = "";
const char* NETWORK_PASSWORD = "***REMOVED***";
const char* LOGGER_ADDRESS = "192.168.0.61";
const int LOGGER_PORT = 1883;
#define NETWORK_CONNECT_TIMEOUT 10
#define LOGGER_CONNECT_TIMEOUT 10
#define LOGGER_SUBSCRIBE_TIMEOUT 8
#define LOGGER_SESSION_TIMEOUT 8
#define LOGGER_REPORT_TIMEOUT 8

// Not user configurable
const gpio_num_t RTC_SQW_PIN = GPIO_NUM_35;
#define PRE_ALARM_SLEEP_TIME 5
#define BUFFER_MAX 10

// Network related
bool awaiting_subscribe = false;
uint16_t subscribe_req_id = 0;
bool awaiting_session = false;
char session_req_id[11] = { '\0' };
bool awaiting_report = false;
char report_req_id[11] = { '\0' };
ReportResult
    report_req_result = ReportResult::None;

// Stored in sleep memory
RTC_DATA_ATTR bool was_sleeping = false;
RTC_DATA_ATTR char mac_address[18] = { '\0' };
RTC_DATA_ATTR int session_id = -1;
RTC_DATA_ATTR int session_interval = -1;
RTC_DATA_ATTR int session_batch_size = -1;
RTC_DATA_ATTR report_buffer_t buffer;
RTC_DATA_ATTR report_t reports[BUFFER_MAX];

// Services
RtcDS3231<TwoWire> rtc(Wire);
AsyncMqttClient logger;


void setup()
{
    Serial.begin(9600);
    rtc.Begin();

    if (!was_sleeping)
    {
        // Get MAC address for unique identification of the node
        uint8_t mac_out[6];
        esp_efuse_mac_get_default(mac_out);

        sprintf(mac_address, "%x:%x:%x:%x:%x:%x", mac_out[0], mac_out[1],
            mac_out[2], mac_out[3], mac_out[4], mac_out[5]);

        // Perform network related setup and config and reboot if anything fails
        // NOTE: I cannot fully guarantee these functions will work properly
        // when called multiple times. The only way to ensure the system is not
        // left in an unrecoverable state is to perform a full MCU restart.
        if (!network_connect() || !update_rtc_time() || !logger_connect()
                || !logger_subscribe() || !logger_session())
            esp_restart();

        // No session for this node so go to sleep permanently
        if (session_id == -1) esp_deep_sleep_start();

        buffer.MAX_SIZE = BUFFER_MAX;
        rtc.SetSquareWavePin(DS3231SquareWavePin_ModeAlarmOne);

        RtcDateTime first_alarm = rtc.GetDateTime();
        first_alarm += (60 - first_alarm.Second());
        first_alarm = round_up(first_alarm, session_interval * 60);

        // Leave at least 5 seconds between sleep and wake
        if (first_alarm - rtc.GetDateTime() <= PRE_ALARM_SLEEP_TIME)
            first_alarm += session_interval * 60;

        // Set alarm for first report and go to sleep
        set_alarm(rtc, first_alarm);
        esp_sleep_enable_ext0_wakeup(RTC_SQW_PIN, 0);
        was_sleeping = true;
        esp_deep_sleep_start();
    }
    else wake_routine();
}

void loop() { }


/*
    Generates a report, sets next alarm, transmits reports, goes to sleep
 */
void wake_routine()
{
    if (!rtc.IsDateTimeValid()) esp_deep_sleep_start();
    
    RtcDateTime now = rtc.GetDateTime();
    RtcDateTime next_alarm = now + (session_interval * 60);
    generate_report(now);

    // Transmit reports in buffer (only if there's enough reports)
    if (buffer.count >= session_batch_size && network_connect()
        && logger_connect() && logger_subscribe())
    {
        // Only transmit if there's enough time before next alarm
        while (!buffer.is_empty() && next_alarm - now
            >= LOGGER_REPORT_TIMEOUT + PRE_ALARM_SLEEP_TIME)
        {
            report_t report = buffer.pop_rear(reports);
            char report_json[128] = { '\0' };
            report_to_string(report_json, report, session_id, mac_address);
            Serial.println(report_json);

            if (!logger_report(report_json, report.time))
            {
                buffer.push_rear(reports, report);
                break;
            }
            else
            {
                // Session has ended so go to sleep permanently
                if (report_req_result == ReportResult::NoSession)
                    esp_deep_sleep_start();
            }
        }
    }

    // Set alarm for next report and go to sleep
    set_alarm(rtc, next_alarm);
    esp_sleep_enable_ext0_wakeup(RTC_SQW_PIN, 0);
    esp_deep_sleep_start();
}

/*
    Samples the sensors, generates a report and adds it to the buffer
 */
void generate_report(const RtcDateTime& time)
{
    report_t report = { time, -99, -99, -99, -99, -99 };

    // Sample airt and relh
    Adafruit_BME680 bme680;
    if (bme680.begin(0x76))
    {
        bme680.setTemperatureOversampling(BME680_OS_8X);
        bme680.setHumidityOversampling(BME680_OS_2X);

        if (bme680.performReading())
        {
            report.airt = bme680.temperature;
            report.relh = bme680.humidity;
        }
    }

    // Sample lvis and lifr
    // report.lvis = ...
    // report.lifr = ...

    // Sample batv
    // report.batv = ...

    buffer.push_front(reports, report);
}


/*
    Connects to wifi network or times out (blocking)
 */
bool network_connect()
{
    WiFi.begin(NETWORK_ID, NETWORK_PASSWORD);
    delay(1000);

    // Check connection status and timeout after set time
    int checks = 1;
    while (WiFi.status() != WL_CONNECTED)
    {
        if (checks++ >= NETWORK_CONNECT_TIMEOUT)
            return false;
        else delay(1000);
    }

    return true;
}

/*
    Connects to MQTT broker or times out (blocking)
 */
bool logger_connect()
{
    if (WiFi.status() != WL_CONNECTED) return false;

    logger.onSubscribe(logger_on_subscribe);
    logger.onMessage(logger_on_message);
    logger.setServer(LOGGER_ADDRESS, LOGGER_PORT);
    logger.connect();
    delay(1000);

    // Check connection status and timeout after set time
    int checks = 1;
    while (!logger.connected())
    {
        if (checks++ >= LOGGER_CONNECT_TIMEOUT)
            return false;
        else delay(1000);
    }

    return true;
}

/*
    Subscribes to inbound topic, waits for response or times out (blocking)
 */
bool logger_subscribe()
{
    if (WiFi.status() != WL_CONNECTED) return false;

    char inbound_topic[64] = { '\0' };
    sprintf(inbound_topic, "nodes/%s/inbound/#", mac_address);
    uint16_t result = logger.subscribe(inbound_topic, 0);

    // Check if successfully sent subscribe request
    if (result)
    {
        subscribe_req_id = result;
        awaiting_subscribe = true;
    } else return false;
    delay(1000);

    // Check result status and timeout after set time
    int checks = 1;
    while (awaiting_subscribe)
    {
        if (checks++ >= LOGGER_SUBSCRIBE_TIMEOUT)
        {
            awaiting_subscribe = false;
            return false;
        } else delay(1000);
    }

    return true;
}

/*
    Sends session request, waits for response or times out (blocking)
 */
bool logger_session()
{
    if (WiFi.status() != WL_CONNECTED) return false;

    char outbound_topic[64] = { '\0' };
    sprintf(session_req_id, "%lu", rtc.GetDateTime() + 946684800UL);
    sprintf(outbound_topic, "nodes/%s/outbound/%s", mac_address,
        session_req_id);

    uint16_t result = logger.publish(outbound_topic, 1, false, "get_session");

    // Check if successfully sent session request
    if (result)
        awaiting_session = true;
    else return false;
    delay(1000);

    // Check result status and timeout after set time
    int checks = 1;
    while (awaiting_session)
    {
        if (checks++ >= LOGGER_SESSION_TIMEOUT)
        {
            awaiting_session = false;
            return false;
        } else delay(1000);
    }

    return true;
}

/*
    Sends report, waits for response or times out (blocking)
 */
bool logger_report(const char* report, uint32_t time)
{
    if (WiFi.status() != WL_CONNECTED) return false;

    char reports_topic[64] = { '\0' };
    sprintf(report_req_id, "%lu", time + 946684800UL);
    sprintf(reports_topic, "nodes/%s/reports/%s", mac_address,
        report_req_id);

    uint16_t result = logger.publish(reports_topic, 0, false, report);

    // Check if successfully sent report
    if (result)
        awaiting_report = true;
    else return false;
    delay(1000);

    // Check result status and timeout after set time
    int checks = 1;
    while (awaiting_report)
    {
        if (checks++ >= LOGGER_REPORT_TIMEOUT)
        {
            awaiting_report = false;
            return false;
        } else delay(1000);
    }

    return true;
}


/*
    Called when the MQTT broker receives a subscription acknowledgement
 */
void logger_on_subscribe(uint16_t packet_id, uint8_t qos)
{
    if (awaiting_subscribe && packet_id == subscribe_req_id)
        awaiting_subscribe = false;
}

/*
    Called when the MQTT broker receives a message
 */
void logger_on_message(char* topic, char* payload,
    AsyncMqttClientMessageProperties properties, size_t length, size_t index,
    size_t total)
{
    // Get the ID of the received message
    char message_id[11] = { '\0' };
    std::string topic_str = std::string(topic);
    std::string topic_substring
        = topic_str.substr(topic_str.find_last_of('/') + 1);
    memcpy(message_id, topic_substring.c_str(), 10);

    // Copy message into memory to remove unwanted trailing characters
    char* message = (char*)calloc(length + 1, sizeof(char));
    memcpy(message, payload, length);

    // Process the received message
    if (awaiting_session)
    {
        if (strcmp(message_id, session_req_id) == 0)
        {
            // If got a session then extract the session info
            if (strcmp(message, "no_session") != 0)
            {
                StaticJsonDocument<300> document;
                DeserializationError deser = deserializeJson(document, message);
                
                if (deser == DeserializationError::Ok)
                {
                    session_id = document["session"];
                    session_interval = document["interval"];
                    session_batch_size = document["batch_size"];
                    awaiting_session = false;
                }
            }
            else awaiting_session = false;
        }
    }
    else if (awaiting_report)
    {
        if (strcmp(message_id, report_req_id) == 0)
        {
            if (strcmp(message, "ok") == 0)
            {
                report_req_result = ReportResult::Ok;
                awaiting_report = false;
            }
            else if (strcmp(message, "no_session") == 0)
            {
                report_req_result = ReportResult::NoSession;
                awaiting_report = false;
            }
        }
    }
}


/*
    Checks if RTC time needs updating and does so via NTP (blocking)
 */
bool update_rtc_time()
{
    bool is_rtc_valid = rtc.IsDateTimeValid();
    if (rtc.LastError() != 0) return false;
    if (is_rtc_valid) return true;

    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiUDP udp;
    NTPClient ntp(udp);
    ntp.begin();

    while (!ntp.forceUpdate())
    {
        if (WiFi.status() != WL_CONNECTED)
            return false;
    }

    // Subtract 30 years since RTC library uses 2000 epoch but NTP returns time
    // relative to 1970 epoch
    rtc.SetDateTime(RtcDateTime(ntp.getEpochTime() - 946684800UL));
    return (rtc.LastError() == 0) ? true : false;
}