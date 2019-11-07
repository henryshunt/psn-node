#include <Arduino.h>
#include <Wire.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include <ctime>

#include "RtcDS3231.h"
#include "NTPClient.h"
#include "AsyncMqttClient.h"
#include "Adafruit_Sensor.h"
#include "Adafruit_BME680.h"

#include "main.h"
#include "helpers.h"
#include "buffer.h"


const char* NETWORK_ID = "***REMOVED***";
const char* NETWORK_PASS = "***REMOVED***";
const char* BROKER_ADDR = "192.168.0.61";
const int BROKER_PORT = 1883;

const gpio_num_t RTC_SQW_PIN = GPIO_NUM_35;
#define BUFFER_MAX 5

RTC_DATA_ATTR bool was_sleeping = false;
RTC_DATA_ATTR char mac_address[18] = { '\0' };
RTC_DATA_ATTR char broker_topic[64] = "nodes/";
RTC_DATA_ATTR report_buffer_t buffer;
RTC_DATA_ATTR report_t reports[BUFFER_MAX];

RTC_DATA_ATTR int report_interval = 1;

#define NETWORK_TIMEOUT 10
#define BROKER_TIMEOUT 10
#define BROKER_PUBLISH_TIMEOUT 5
#define PRE_ALARM_SLEEP_TIME 5

RtcDS3231<TwoWire> rtc(Wire);
AsyncMqttClient broker;


void setup()
{
    Serial.begin(9600);

    rtc.Begin();
    rtc.Enable32kHzPin(false);
    rtc.SetSquareWavePin(DS3231SquareWavePin_ModeAlarmOne);

    if (!was_sleeping)
    {
        // Get MAC address for unique node identification
        uint8_t mac_temp[6];
        esp_efuse_mac_get_default(mac_temp);

        sprintf(mac_address, "%x:%x:%x:%x:%x:%x", mac_temp[0],
            mac_temp[1], mac_temp[2], mac_temp[3], mac_temp[4], mac_temp[5]);
        strcat(broker_topic, mac_address);
    
        // Don't continue until connected to Wifi
        while (!connect_network());

        // Don't continue until RTC time valid (update if required)
        while (true)
        {
            bool isRTCValid = rtc.IsDateTimeValid();

            if (rtc.LastError() == 0)
            {
                if (!isRTCValid) set_rtc_time();
                break;
            }
        }

        // Don't continue until connected to MQTT broker
        while (!connect_broker());

        buffer.MAX_SIZE = BUFFER_MAX;

        // Set alarm to trigger the first report then enter deep sleep
        RtcDateTime first_alarm = rtc.GetDateTime();
        first_alarm += (60 - first_alarm.Second());
        first_alarm = round_up(first_alarm, report_interval * 60);

        if (first_alarm - rtc.GetDateTime() <= PRE_ALARM_SLEEP_TIME)
            first_alarm += report_interval * 60;
        set_alarm(rtc, first_alarm);

        was_sleeping = true;
        esp_sleep_enable_ext0_wakeup(RTC_SQW_PIN, 0);
        esp_deep_sleep_start();
    }
    else
    {
        was_sleeping = false;
        wakeup_routine();
    }
}

void loop() { }

/*
    Generates a report, sets next alarm, transmits reports, goes to sleep
 */
void wakeup_routine()
{
    RtcDateTime now = rtc.GetDateTime();

    // Generate report and set alarm for next report
    generate_report(now);
    RtcDateTime next_alarm = now + (report_interval * 60);
    set_alarm(rtc, next_alarm);

    // Transmit reports in buffer
    if (!buffer.is_empty() && connect_network() && connect_broker())
    {
        // Check if transmit may clash with next alarm
        while (!buffer.is_empty() && next_alarm - now
            >= BROKER_PUBLISH_TIMEOUT + PRE_ALARM_SLEEP_TIME)
        {
            report_t report = buffer.pop_rear(reports);
            char report_json[256] = { '\0' };

            report_to_string(report_json, report, mac_address);
            Serial.println(report_json);

            if (!broker_publish(broker_topic, report_json))
            {
                buffer.push_rear(reports, report);
                break;
            }
        }
    }

    // Enter deep sleep
    was_sleeping = true;
    esp_sleep_enable_ext0_wakeup(RTC_SQW_PIN, 0);
    esp_deep_sleep_start();
}


/*
    Samples the sensors, generates a report and adds it to the buffer
 */
void generate_report(const RtcDateTime& time)
{
    report_t report;
    report.time = time;

    // Sample airt and relh
    Adafruit_BME680 bme680;
    if (bme680.begin(0x76))
    {
        bme680.setTemperatureOversampling(BME680_OS_8X);
        bme680.setHumidityOversampling(BME680_OS_2X);

        if (bme680.performReading())
        {
            report.airt = bme680.temperature;
            report.airt_ok = true;
            report.relh = bme680.humidity;
            report.relh_ok = true;
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
    Blocks until connected to Wifi or times out after 10 seconds
 */
bool connect_network()
{
    WiFi.begin(NETWORK_ID, NETWORK_PASS);
    delay(1000);
    
    int checks = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
        if (checks++ < NETWORK_TIMEOUT - 1)
            delay(1000);
        else
        {
            WiFi.disconnect();
            return false;
        }
    }

    return true;
}

/*
    Blocks until connected to MQTT broker or times out after 10 seconds
 */
bool connect_broker()
{
    broker.setServer(BROKER_ADDR, BROKER_PORT);
    broker.connect();
    delay(1000);

    int checks = 0;
    while (!broker.connected())
    {
        if (checks++ < BROKER_TIMEOUT - 1)
            delay(1000);
        else
        {
            broker.disconnect();
            return false;
        }
    }

    return true;
}

/*
    Blocks until message is published or times out after 10 seconds
 */
bool broker_publish(const char* topic, const char* message)
{
    int result = -1;
    result = broker.publish(broker_topic, 1, false, message);
    delay(1000);
    
    int checks = 0;
    while (result <= 0)
    {
        if (result == 0) return false;
        if (result == -1)
        {
            if (checks++ < BROKER_PUBLISH_TIMEOUT - 1)
                delay(1000);
            else
            {
                broker.disconnect();
                return false;
            }
        }
    }

    return true;
}


/*
    Blocks until RTC time has been set to time retrieved via NTP
 */
void set_rtc_time()
{
    WiFiUDP udp;
    NTPClient ntp(udp);

    // Keep trying until successfully got time
    ntp.begin();

    while (true)
    {
        bool update = ntp.forceUpdate();

        if (update)
        {
            // Subtract 30 years since RTC library uses 2000 epoch but NTP
            // returns time relative to 1970 epoch
            rtc.SetDateTime(RtcDateTime(ntp.getEpochTime() - 946684800UL));
            if (rtc.LastError() == 0) break;
        }
    }

    ntp.end();
}