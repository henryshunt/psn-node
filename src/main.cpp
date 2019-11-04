#include <Arduino.h>
#include <Wire.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include <ctime>

#include "CircularBuffer.h"
#include "RtcDS3231.h"
#include "NTPClient.h"
#include "PubSubClient.h"
#include "Adafruit_Sensor.h"
#include "Adafruit_BME680.h"

#include "main.h"
#include "helpers.h"


const char* NETWORK_ID = "***REMOVED***"; // Wifi SSID
const char* NETWORK_PASS = "***REMOVED***"; // Wifi password
const char* BROKER_ADDR = "192.168.0.61"; // MQTT broker address
const int BROKER_PORT = 1883; // MQTT broker port
const char* BROKER_BASE = "nodes/"; // MQTT publish topic base path
const int RTC_SQW_PIN = 19; // Real time clock square wave input pin
const int BUFFER_LIMIT = 10; // Maximum reports to keep in transmit buffer

char mac_address[18] = { '\0' }; // String for storing the MAC address
char* broker_topic; // String for storing the topic to publish to
bool has_setup = false; // Has the setup method run?
bool should_report = false; // Should the loop generate a report?

int report_interval = 1; // Stores the retrieved reporting interval

CircularBuffer<char*, BUFFER_LIMIT> reports; // Stores reports to transmit

RtcDS3231<TwoWire> rtc(Wire);
WiFiClient network;
PubSubClient broker(network);


void setup()
{
    Serial.begin(9600);

    // Get MAC address for identifying the node
    uint8_t mac_temp[6];
    esp_efuse_mac_get_default(mac_temp);

    sprintf(mac_address, "%x:%x:%x:%x:%x:%x", mac_temp[0],
        mac_temp[1], mac_temp[2], mac_temp[3], mac_temp[4], mac_temp[5]);

    // Create MQTT topic from base path and MAC address
    broker_topic = (char*)calloc(
        strlen(BROKER_BASE) + strlen(mac_address) + 1, sizeof(char));
    strcpy(broker_topic, BROKER_BASE);
    strcat(broker_topic, mac_address);

    // Don't continue until connected to Wifi
    while (!network_connect());

    rtc.Begin();
    rtc.Enable32kHzPin(false);
    rtc.SetSquareWavePin(DS3231SquareWavePin_ModeAlarmOne);
    attachInterrupt(RTC_SQW_PIN, alarm_triggered, FALLING);

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
    Serial.println("online");

    // Set alarm to trigger the first report
    RtcDateTime first_alarm = rtc.GetDateTime();
    first_alarm += (60 - first_alarm.Second()); // Start of next minute
    first_alarm = round_up(first_alarm, report_interval * 60);
    has_setup = true;
    set_alarm(rtc, first_alarm);

    // No point sleeping if there's only 10 seconds until wake-up
    if (first_alarm - rtc.GetDateTime() >= 10)
    {
        // Sleep MCU
    }
}

void loop()
{
    if (should_report)
    {
        RtcDateTime now = rtc.GetDateTime();
        should_report = false;

        // Generate new report and set alarm for next report
        generate_report(now);
        RtcDateTime next_alarm = now + (report_interval * 60);
        set_alarm(rtc, next_alarm);

        // Transmit the report
        if (reports.isEmpty()) return;

        if (network_connect() && broker_connect())
        {
            // Check if transmit may clash with next alarm
            while (!reports.isEmpty() && next_alarm - now >= 10)
            {
                char* report = reports.pop();
                if (!broker.publish(broker_topic, report))
                {
                    reports.push(report);
                    break;
                }
                else
                {
                    Serial.println(report);
                    free(report);
                }
            }
        }
    }
}


/*
    Fires when an alarm on the RTC has been triggered
 */
void alarm_triggered()
{
    if (!has_setup) return;
    should_report = true;
}


/*
    Samples the sensors, generates a report and adds it to the buffer
 */
void generate_report(const RtcDateTime& now)
{
    char* report = (char*)calloc(256, sizeof(char));

    // Sample airt and relh
    Adafruit_BME680 bme680;
    bool airt_sampled = false;
    float airt;
    bool relh_sampled = false;
    float relh;

    if (bme680.begin(0x76))
    {
        bme680.setTemperatureOversampling(BME680_OS_8X);
        bme680.setHumidityOversampling(BME680_OS_2X);

        if (bme680.performReading())
        {
            airt_sampled = true;
            airt = bme680.temperature;
            relh_sampled = true;
            relh = bme680.humidity;
        }
    }

    // Sample lvis and lifr
    bool lvis_sampled = false;
    int lvis;
    bool lifr_sampled = false;
    int lifr;

    // Sample batv
    bool batv_sampled = false;
    float batv;


    // Format the report timestamp
    char time[32] = { '\0' };
    sprintf(time, "%04u-%02u-%02u %02u:%02u:%02u", now.Year(), now.Month(),
        now.Day(), now.Hour(), now.Minute(), now.Second());

    // Generate the report
    sprintf(report,"{ \"node\": \"%s\", \"time\": \"%s\"", mac_address, time);

    if (airt_sampled)
        concat_report_value<float>(report, ", \"airt\": %.1f", airt);
    else strcat(report, ", \"airt\": null");

    if (relh_sampled)
        concat_report_value<float>(report, ", \"relh\": %.1f", relh);
    else strcat(report, ", \"relh\": null");

    if (lvis_sampled)
        concat_report_value<float>(report, ", \"lvis\": %.1f", lvis);
    else strcat(report, ", \"lvis\": null");

    if (lifr_sampled)
        concat_report_value<float>(report, ", \"lifr\": %.1f", lifr);
    else strcat(report, ", \"lifr\": null");

    if (batv_sampled)
        concat_report_value<float>(report, ", \"batv\": %.1f", batv);
    else strcat(report, ", \"batv\": null");

    strcat(report, " }");
    reports.push(report);
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