#include <nvs_flash.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_wpa2.h>
#include <Wire.h>
#include <WiFiUdp.h>

#include "RtcDS3231.h"
#include "NTPClient.h"
#include "AsyncMqttClient.h"
#include "Adafruit_Sensor.h"
#include "Adafruit_BME680.h"
#include "Adafruit_TSL2591.h"
#include "ArduinoJson.h"

#include "main.h"
#include "helpers.h"
#include "buffer.h"


// Public configuration
RTC_DATA_ATTR bool NETWORK_ENTERPRISE = false;
RTC_DATA_ATTR char NETWORK_NAME[32] = { '\0' };
RTC_DATA_ATTR char NETWORK_USERNAME[64] = { '\0' };
RTC_DATA_ATTR char NETWORK_PASSWORD[64] = { '\0' };
RTC_DATA_ATTR char LOGGER_ADDRESS[32] = { '\0' };
RTC_DATA_ATTR uint16_t LOGGER_PORT = 1883;
RTC_DATA_ATTR uint8_t NETWORK_CONNECT_TIMEOUT = 10;
RTC_DATA_ATTR uint8_t LOGGER_CONNECT_TIMEOUT = 10;
RTC_DATA_ATTR uint8_t LOGGER_SUBSCRIBE_TIMEOUT = 8;
RTC_DATA_ATTR uint8_t LOGGER_SESSION_TIMEOUT = 8;
RTC_DATA_ATTR uint8_t LOGGER_REPORT_TIMEOUT = 8;

// Private configuration
#define RTC_SQW_PIN GPIO_NUM_35
#define SERIAL_CONNECT_TIMEOUT 10
#define PRE_ALARM_SLEEP_TIME 5
#define BUFFER_MAX 10

// Network related
bool awaiting_subscribe = false;
uint16_t subscribe_req_id = -1;
bool awaiting_session = false;
char session_req_id[11] = { '\0' };
bool awaiting_report = false;
char report_req_id[11] = { '\0' };
ReportResult report_req_result = ReportResult::None;

// Persisted between deep sleeps
RTC_DATA_ATTR bool cold_booted = true;
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


void setup()
{
    Serial.begin(9600);
    rtc.Begin();

    if (cold_booted)
    {
        // Get MAC address for unique identification of the node
        uint8_t mac_out[6];
        esp_efuse_mac_get_default(mac_out);

        sprintf(mac_address, "%x:%x:%x:%x:%x:%x", mac_out[0], mac_out[1],
            mac_out[2], mac_out[3], mac_out[4], mac_out[5]);

        // Load device configuration from non-volatile storage
        if (!config.begin("psn", false)) esp_restart();
        
        bool config_result = load_configuration();

        // Enter serial mode if data received within a set time after boot
        bool serial_mode = true;
        delay(1000);

        int checks = 1;
        while (!Serial.available())
        {
            if (checks++ >= SERIAL_CONNECT_TIMEOUT)
            {
                serial_mode = false;
                break;
            }
            else delay(1000);
        }

        if (serial_mode) serial_routine();

        // Configuration is invalid so we can't proceed
        if (!config_result) esp_deep_sleep_start();

        // Connect to network and logging server and reboot on failure
        // NOTE: I cannot fully guarantee these functions will work properly
        // when called multiple times. The only way to ensure the system is not
        // left in an unrecoverable state is to perform a full restart.
        if (!network_connect() || !logger_connect()) esp_restart();

        // Update RTC time if required and restart device if disconnected
        while (true)
        {
            if (!update_rtc_time())
            {
                if (WiFi.status() != WL_CONNECTED) esp_restart();
            } else break;
        }

        // Subscribe to inbound topic and restart device if disconnected
        while (true)
        {
            if (!logger_subscribe())
            {
                if (WiFi.status() != WL_CONNECTED || !logger.connected())
                    esp_restart();
            } else break;
        }

        // Request session for this node and restart device if disconnected
        while (true)
        {
            if (!logger_session())
            {
                if (WiFi.status() != WL_CONNECTED || !logger.connected())
                    esp_restart();
            } else break;
        }

        // No session for this node so go to sleep permanently
        if (session_id == -1) esp_deep_sleep_start();

        buffer.MAX_SIZE = BUFFER_MAX;
        rtc.SetSquareWavePin(DS3231SquareWavePin_ModeAlarmOne);

        RtcDateTime first_alarm = rtc.GetDateTime();
        first_alarm += (60 - first_alarm.Second());
        first_alarm = round_up(first_alarm, session_interval * 60);

        // Leave some time between sleep and wake
        if (first_alarm - rtc.GetDateTime() <= PRE_ALARM_SLEEP_TIME)
            first_alarm += session_interval * 60;

        // Set alarm for first report and go into deep sleep
        set_alarm(rtc, first_alarm);
        esp_sleep_enable_ext0_wakeup(RTC_SQW_PIN, 0);
        cold_booted = false;
        esp_deep_sleep_start();
    }
    else wake_routine();
}

void loop() { }


/*
    Sits in a continuous loop and responds to commands sent over serial
 */
void serial_routine()
{
    while (true)
    {
        char command[200] = { '\0' };
        int position = 0;
        bool line_ended = false;

        // Store received characters until we receive a new line character
        while (!line_ended)
        {
            while (Serial.available())
            {
                char read_char = Serial.read();
                if (read_char != '\n')
                    command[position++] = read_char;
                else line_ended = true;
            }
        }
        
        // Respond to ping command command
        if (strncmp(command, "psna_pn", 7) == 0)
            Serial.write("psna_pnr\n");
        
        // Respond to read configuration command
        else if (strncmp(command, "psna_rc", 7) == 0)
        {
            char config[200] = { '\0' };
            int length = 0;
    
            length += sprintf(config, "psna_rcr { \"madr\": \"");
            length += sprintf(config + length, mac_address);
            length += sprintf(
                config + length, "\", \"nent\": %d", NETWORK_ENTERPRISE);

            if (NETWORK_NAME[0] != '\0')
            {
                length += sprintf(
                    config + length, ", \"nnam\": \"%s\"", NETWORK_NAME);
            } else length += sprintf(config + length, ", \"nnam\": null");

            if (NETWORK_USERNAME[0] != '\0')
            {
                length += sprintf(
                    config + length, ", \"nunm\": \"%s\"", NETWORK_USERNAME);
            } else length += sprintf(config + length, ", \"nunm\": null");

            if (NETWORK_PASSWORD[0] != '\0')
            {
                length += sprintf(
                    config + length, ", \"npwd\": \"%s\"", NETWORK_PASSWORD);
            } else length += sprintf(config + length, ", \"npwd\": null");

            if (LOGGER_ADDRESS[0] != '\0')
            {
                length += sprintf(
                    config + length, ", \"ladr\": \"%s\"", LOGGER_ADDRESS);
            } else length += sprintf(config + length, ", \"ladr\": null");

            length += sprintf(config + length, ", \"lprt\": %u", LOGGER_PORT);
            length += sprintf(
                config + length, ", \"tncn\": %u", NETWORK_CONNECT_TIMEOUT);
            length += sprintf(
                config + length, ", \"tlcn\": %u", LOGGER_CONNECT_TIMEOUT);
            length += sprintf(
                config + length, ", \"tlsb\": %u", LOGGER_SUBSCRIBE_TIMEOUT);
            length += sprintf(
                config + length, ", \"tlss\": %u", LOGGER_SESSION_TIMEOUT);
            length += sprintf(
                config + length, ", \"tlrp\": %u", LOGGER_REPORT_TIMEOUT);
            strcat(config + length, " }\n");
            
            Serial.write(config);
        }

        // Respond to write configuration command
        else if (strncmp(command, "psna_wc {", 9) == 0)
        {
            StaticJsonDocument<300> document;
            DeserializationError deser = deserializeJson(document, command + 8);
            
            if (deser == DeserializationError::Ok)
            {
                if (!config.begin("psn", false))
                {
                    Serial.write("psna_wcf\n");
                    return;
                }

                config.putBool("nent", document["nent"]);
                config.putString("nnam", (const char*)document["nnam"]);
                config.putString("nunm", (const char*)document["nunm"]);
                config.putString("npwd", (const char*)document["npwd"]);
                config.putString("ladr", (const char*)document["ladr"]);
                config.putUShort("lprt", document["lprt"]);
                config.putUChar("tncn", document["tncn"]);
                config.putUChar("tlcn", document["tlcn"]);
                config.putUChar("tlsb", document["tlsb"]);
                config.putUChar("tlss", document["tlss"]);
                config.putUChar("tlrp", document["tlrp"]);
                config.end();
                
                Serial.write("psna_wcs\n");
            }
            else Serial.write("psna_wcf\n");
        }
    }
}

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
            report_to_string(report_json, report, session_id);
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

    // Set alarm for next report and go into deep sleep
    set_alarm(rtc, next_alarm);
    esp_sleep_enable_ext0_wakeup(RTC_SQW_PIN, 0);
    esp_deep_sleep_start();
}

/*
    Samples the sensors, generates a report and adds it to the buffer
 */
void generate_report(const RtcDateTime& time)
{
    report_t report = { time, -99, -99, -99, -99 };

    // Sample temperature and humidity
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

    // Sample light level
    Adafruit_TSL2591 tsl2591;
    if (tsl2591.begin())
    {
        tsl2591.setTiming(TSL2591_INTEGRATIONTIME_200MS);
        tsl2591.setGain(TSL2591_GAIN_MED);

        sensors_event_t event;
        if (tsl2591.getEvent(&event) && event.light > 0)
            report.lght = event.light;
    }

    // Sample battery voltage
    // report.batv = ...

    buffer.push_front(reports, report);
}


/*
    Connects to wifi network or times out (blocking)
 */
bool network_connect()
{
    // Configure for enterprise wifi network if required
    if (NETWORK_ENTERPRISE)
    {
        WiFi.mode(WIFI_STA);
        esp_wifi_sta_wpa2_ent_set_username(
            (uint8_t *)NETWORK_USERNAME, strlen(NETWORK_USERNAME));
        esp_wifi_sta_wpa2_ent_set_password(
            (uint8_t *)NETWORK_PASSWORD, strlen(NETWORK_PASSWORD));
        esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT();
        esp_wifi_sta_wpa2_ent_enable(&config);
    }

    WiFi.begin(NETWORK_NAME, NETWORK_PASSWORD);
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
            subscribe_req_id = -1;
            awaiting_subscribe = false;
            return false;
        } else delay(1000);
    }

    subscribe_req_id = -1;
    return true;
}

/*
    Sends session request, waits for response or times out (blocking)
 */
bool logger_session()
{
    char outbound_topic[64] = { '\0' };
    sprintf(session_req_id, "%lu", rtc.GetDateTime() + 946684800UL);
    sprintf(outbound_topic, "nodes/%s/outbound/%s", mac_address,
        session_req_id);

    uint16_t result = logger.publish(outbound_topic, 1, false, "get_session");

    // Check if successfully sent session request
    if (!result)
    {
        for (int i = 0; i < 11; i++)
            session_req_id[i] = '\0';
        return false;
    }
    else awaiting_session = true;
    delay(1000);

    // Check result status and timeout after set time
    int checks = 1;
    while (awaiting_session)
    {
        if (checks++ >= LOGGER_SESSION_TIMEOUT)
        {
            for (int i = 0; i < 11; i++)
                session_req_id[i] = '\0';
            awaiting_session = false;
            return false;
        } else delay(1000);
    }

    for (int i = 0; i < 11; i++)
        session_req_id[i] = '\0';
    return true;
}

/*
    Sends report, waits for response or times out (blocking)
 */
bool logger_report(const char* report, uint32_t time)
{
    report_req_result = ReportResult::None;

    char reports_topic[64] = { '\0' };
    sprintf(report_req_id, "%lu", time + 946684800UL);
    sprintf(reports_topic, "nodes/%s/reports/%s", mac_address,
        report_req_id);

    uint16_t result = logger.publish(reports_topic, 0, false, report);

    // Check if successfully sent report
    if (!result)
    {
        for (int i = 0; i < 11; i++)
            report_req_id[i] = '\0';
        report_req_result = ReportResult::None;
        return false;
    }
    else awaiting_report = true;
    delay(1000);

    // Check result status and timeout after set time
    int checks = 1;
    while (awaiting_report)
    {
        if (checks++ >= LOGGER_REPORT_TIMEOUT)
        {
            for (int i = 0; i < 11; i++)
                report_req_id[i] = '\0';
            report_req_result = ReportResult::None;
            awaiting_report = false;
            return false;
        } else delay(1000);
    }

    for (int i = 0; i < 11; i++)
        report_req_id[i] = '\0';
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
    Loads configuration from NVS into global variables and checks if valid
 */
bool load_configuration()
{
    NETWORK_ENTERPRISE = config.getBool("nent", false);
    strcpy(NETWORK_NAME, config.getString("nnam", String()).c_str());
    strcpy(NETWORK_USERNAME, config.getString("nunm", String()).c_str());
    strcpy(NETWORK_PASSWORD, config.getString("npwd", String()).c_str());
    strcpy(LOGGER_ADDRESS, config.getString("ladr", String()).c_str());
    LOGGER_PORT = config.getUShort("lprt", 1883);
    NETWORK_CONNECT_TIMEOUT = config.getUChar("tncn", 10);
    LOGGER_CONNECT_TIMEOUT = config.getUChar("tlcn", 10);
    LOGGER_SUBSCRIBE_TIMEOUT = config.getUChar("tlsb", 8);
    LOGGER_SESSION_TIMEOUT = config.getUChar("tlss", 8);
    LOGGER_REPORT_TIMEOUT = config.getUChar("tlrp", 8);
    config.end();

    if (strlen(NETWORK_NAME) == 0 || strlen(NETWORK_PASSWORD) == 0 ||
            (NETWORK_ENTERPRISE && strlen(NETWORK_USERNAME) == 0) ||
            strlen(LOGGER_ADDRESS) == 0)        
        return false;
    else return true;
}

/*
    Checks if RTC time needs updating and attempts to do so via NTP (blocking)
 */
bool update_rtc_time()
{
    bool is_rtc_valid = rtc.IsDateTimeValid();
    if (rtc.LastError() != 0) return false;
    if (is_rtc_valid) return true;

    WiFiUDP udp;
    NTPClient ntp(udp);
    ntp.begin();

    if (!ntp.forceUpdate()) return false;

    // Subtract 30 years since RTC library uses 2000 epoch but NTP returns time
    // relative to 1970 epoch
    rtc.SetDateTime(RtcDateTime(ntp.getEpochTime() - 946684800UL));
    return (rtc.LastError() == 0) ? true : false;
}