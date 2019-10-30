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


const char* NETWORK_ID = "***REMOVED***"; // Wifi SSID
const char* NETWORK_PASS = "***REMOVED***"; // Wifi password
const char* BROKER_ADDR = "192.168.0.61"; // MQTT broker address
const int BROKER_PORT = 1883; // MQTT broker port
const char* BROKER_BASE = "nodes/"; // MQTT publish topic base path

char mac_address[18] = { '\0' }; // String for storing the MAC address
char* broker_topic; // String for storing the topic to publish to

RtcDS3231<TwoWire> rtc(Wire);
WiFiClient network;
PubSubClient mqtt(network);


void update_rtc_time();
void sample_sensors();
bool network_connect();
bool broker_connect();

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
    // while (!network_connect());
    
    // rtc.Begin();

    // Update RTC stored time if needed (loop until success)
    // while (true)
    // {
    //     bool isRTCValid = rtc.IsDateTimeValid();

    //     if (rtc.LastError() == 0)
    //     {
    //         if (!isRTCValid) update_rtc_time();
    //         break;
    //     }
    // }

    // Don't continue until connected to MQTT broker
    // while (!broker_connect())
    
    sample_sensors();
}

void loop()
{
    // RtcDateTime time = rtc.GetDateTime();
}


void sample_sensors()
{
    // RtcDateTime now = rtc.GetDateTime();

    // Sample airt and relh
    Adafruit_BME680 bme;
    bool airt_sampled = false;
    float airt;
    bool relh_sampled = false;
    float relh;

    if (bme.begin(0x76))
    {
        bme.setTemperatureOversampling(BME680_OS_8X);
        bme.setHumidityOversampling(BME680_OS_2X);

        if (bme.performReading())
        {
            airt_sampled = true;
            airt = bme.temperature;
            relh_sampled = true;
            relh = bme.humidity;
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


    // // Format the report timestamp
    // char time[20] = { '\0' };
    // sprintf(time, "%04u-%02ud-%02u %02u:%02u:%02u", now.Year(), now.Month(),
    //     now.Day(), now.Hour(), now.Minute(), now.Second());

    // Generate the report
    // char report[256] = { '\0' };
    // const char* report_base = "{ \"node\": \"%s\", \"airt\": %.1f, "
    //     "\"relh\": %.1f, \"lvis\": %d, \"lifr\": %d, \"batv\": %.2f, \"sigs\": %d }";

    // sprintf(report, report_base, mac_address, airt, relh, lvis, lifr,
    //     batv, sigs);
    // Serial.println(report);


    char report[256] = { '\0' };
    sprintf(report, "{ \"node\": \"%s\", \"time\": \"%s\"", mac_address, "test");

    if (airt_sampled)
    {
        char section[32] = { '\0' };
        sprintf(section, ", \"airt\": %.1f", airt);
        strcat(report, section);
    }
    else strcat(report, ", \"airt\": null");

    if (relh_sampled)
    {
        char section[32] = { '\0' };
        sprintf(section, ", \"relh\": %.1f", relh);
        strcat(report, section);
    }
    else strcat(report, ", \"relh\": null");

    if (lvis_sampled)
    {
        char section[32] = { '\0' };
        sprintf(section, ", \"lvis\": %d", lvis);
        strcat(report, section);
    }
    else strcat(report, ", \"lvis\": null");

    if (lifr_sampled)
    {
        char section[32] = { '\0' };
        sprintf(section, ", \"lifr\": %d", lifr);
        strcat(report, section);
    }
    else strcat(report, ", \"lifr\": null");

    if (batv_sampled)
    {
        char section[32] = { '\0' };
        sprintf(section, ", \"batv\": %.2f", batv);
        strcat(report, section);
    }
    else strcat(report, ", \"batv\": null");

    strcat(report, " }");
    Serial.println(report);

    // // Transmit the report
    // if (network_connect() && broker_connect())
    //     mqtt_client.publish(broker_topic, report);
}

void transmit_reports()
{
    
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
    Attempts to connect to the MQTT broker once
 */
bool broker_connect()
{
    mqtt.setServer(BROKER_ADDR, BROKER_PORT);
    mqtt.connect(broker_topic);
    return mqtt.connected();
}


/*
    Blocks until RTC time has been sucessfully updated via NTP
 */
void update_rtc_time()
{
    WiFiUDP udp;
    NTPClient ntp(udp);

    ntp.begin();    
    while (!ntp.forceUpdate())
    ntp.end();

    //rtc.adjust(DateTime(__DATE__, __TIME__));
}