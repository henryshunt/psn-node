#include <Arduino.h>
#include <Wire.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include <ctime>

#include "RtcDS3231.h"
#include "NTPClient.h"
#include "PubSubClient.h"
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

RtcDS3231<TwoWire> rtc(Wire);
WiFiClient network;
PubSubClient broker(network);


void setup()
{
    Serial.begin(9600);

    rtc.Begin();
    rtc.Enable32kHzPin(false);
    rtc.SetSquareWavePin(DS3231SquareWavePin_ModeAlarmOne);


    // If clean boot else woke from sleep
    if (!was_sleeping)
    {
        // Get MAC address for unique node identification
        uint8_t mac_temp[6];
        esp_efuse_mac_get_default(mac_temp);

        sprintf(mac_address, "%x:%x:%x:%x:%x:%x", mac_temp[0],
            mac_temp[1], mac_temp[2], mac_temp[3], mac_temp[4], mac_temp[5]);
        strcat(broker_topic, mac_address);

        // Don't continue until connected to Wifi
        while (!network_connect());

        // Update RTC stored time if needed (loop until success)
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
        while (!broker_connect());

        buffer.MAX_SIZE = BUFFER_MAX;

        // Set alarm to trigger the first report then enter deep sleep
        RtcDateTime first_alarm = rtc.GetDateTime();
        first_alarm += (60 - first_alarm.Second());
        first_alarm = round_up(first_alarm, report_interval * 60);
        set_alarm(rtc, first_alarm);

        was_sleeping = true;
        esp_sleep_enable_ext0_wakeup(RTC_SQW_PIN, 0);
        esp_deep_sleep_start();
    }
    else
    {
        was_sleeping = false;
        wake_routine();
    }
}

void loop() { }


void wake_routine()
{
    RtcDateTime now = rtc.GetDateTime();
    Serial.println("wake");

    // Generate report and set alarm for next report
    generate_report(now);
    RtcDateTime next_alarm = now + (report_interval * 60);
    set_alarm(rtc, next_alarm);

    // Transmit reports in buffer
    if (!buffer.is_empty() && network_connect() && broker_connect())
    {
        // Check if transmit may clash with next alarm
        while (!buffer.is_empty() && next_alarm - now >= 10)
        {
            report_t report = buffer.pop_rear(reports);
            char report_json[256] = { '\0' };

            report_to_string(report_json, report, mac_address);
            Serial.println(report_json);

            if (!broker.publish(broker_topic, report_json))
            {
                Serial.println("fail");
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
    Blocks until connected to Wifi or timeout after 10 seconds
 */
bool network_connect()
{
    if (WiFi.status() == WL_CONNECTED) return true;

    int checks = 0;
    WiFi.begin(NETWORK_ID, NETWORK_PASS);

    // Check connection status and timeout after 10 seconds
    while (WiFi.status() != WL_CONNECTED)
    {
        if (checks++ < 11)
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
    Attempts to connect to the MQTT broker
 */
bool broker_connect()
{
    if (broker.connected()) return true;

    broker.setServer(BROKER_ADDR, BROKER_PORT);
    broker.connect(broker_topic);
    return broker.connected();
}


/*
    Blocks until RTC time has been sucessfully updated via NTP
 */
void set_rtc_time()
{
    WiFiUDP udp;
    NTPClient ntp(udp);

    // Keep trying until successfully got time
    ntp.begin();
    while (!ntp.forceUpdate())
    ntp.end();

    // Subtract 30 years since RTC library uses 2000 epoch but NTP returns time
    // relative to 1970 epoch
    rtc.SetDateTime(RtcDateTime(ntp.getEpochTime() - 946684800UL));
}