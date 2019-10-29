#include <Arduino.h>
#include <Wire.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include <ctime>

#include "RtcDS3231.h"
#include "NTPClient.h"
#include "PubSubClient.h"


const char* NETWORK_ID = "***REMOVED***"; // WiFi SSID
const char* NETWORK_PASS = "***REMOVED***"; // WiFi password
const char* BROKER_ADDR = "192.168.0.61"; // MQTT broker address
const int BROKER_PORT = 1883; // MQTT broker port
const char* BROKER_BASE = "nodes/"; // MQTT publish topic base path

char mac_address[18] = { '\0' }; // String for storing the MAC address
char* broker_topic; // String for storing the topic to publish to

RtcDS3231<TwoWire> rtc(Wire);
WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP);
WiFiClient network;
PubSubClient mqtt_client(network);


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

    Serial.println(network_connect());

    // rtc.Begin();

    // Update RTC stored time if needed (check until success)
    // while (true)
    // {
    //     bool isRTCValid = rtc.IsDateTimeValid();

    //     if (rtc.LastError() == 0)
    //     {
    //         if (!isRTCValid) update_rtc_time();
    //         break;
    //     }
    // }


    // sample_sensors();
}

void loop()
{
    // RtcDateTime time = rtc.GetDateTime();
}


void sample_sensors()
{
    RtcDateTime now = rtc.GetDateTime();

    // Sample each sensor
    float airt = 13.3;
    float relh = 93.6;
    int lvis = 88;
    int lifr = 72;
    float batv = 2.54;
    int sigs = WiFi.RSSI();

    // Format the report timestamp
    char time[20] = { '\0' };
    sprintf(time, "%04u-%02ud-%02u %02u:%02u:%02u", now.Year(), now.Month(),
        now.Day(), now.Hour(), now.Minute(), now.Second());

    // Generate the report
    char report[256] = { '\0' };
    const char* report_base = "{ \"node\": \"%s\", \"time\": \"%s\", \"airt\": %.1f, "
        "\"relh\": %.1f, \"lvis\": %d, \"lifr\": %d, \"batv\": %.2f, \"sigs\": %d }";

    sprintf(report, report_base, mac_address, time, airt, relh, lvis, lifr,
        batv, sigs);
    Serial.println(report);

    // Transmit the report
    if (network_connect() && broker_connect())
        mqtt_client.publish(broker_topic, report);
}

/*
    Blocks until connected to WiFi or timeout after 10 seconds
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

bool broker_connect()
{
    mqtt_client.setServer(BROKER_ADDR, BROKER_PORT);
    mqtt_client.connect(broker_topic);
    return mqtt_client.connected();
}


/*
    Blocks until RTC time has been sucessfully updated via NTP
 */
void update_rtc_time()
{
    while (true)
    {
        if (network_connect())
        {
            ntpClient.begin();
            delay(1500);
            
            while (ntpClient.forceUpdate() == false)
            ntpClient.end();

            //rtc.adjust(DateTime(__DATE__, __TIME__));
            return;
        }
    }
}