#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ctime>
#include <EEPROM.h>
#include <Wire.h>

#include "PubSubClient.h"
#include "RTClib.h"
#include "NTPClient.h"


const char* NETWORK_ID = "***REMOVED***";
const char* NETWORK_PASS = "***REMOVED***";
const char* BROKER_ADDR = "192.168.0.61";
const int BROKER_PORT = 1883;
const char* TOPIC_BASE = "nodes/";


char mac_address[18];
char* node_topic;

WiFiClient network;
PubSubClient mqtt_client(network);
WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP);

const char* report_base = "{ \"node\": \"%s\", \"time\": \"%s\", " "\"airt\": %.1f, "
    "\"relh\": %.1f, \"lvis\": %d, \"lifr\": %d, \"batv\": %.2f, \"sigs\": %d }";
char report[256];


void sample_sensors();
bool network_connect();
bool broker_connect();

void setup()
{
    Serial.begin(9600);
    //rtc.begin();
    
    // Get MAC address to identify node
    uint8_t mac_temp[6];
    esp_efuse_mac_get_default(mac_temp);

    sprintf(mac_address, "%x:%x:%x:%x:%x:%x", mac_temp[0],
        mac_temp[1], mac_temp[2], mac_temp[3], mac_temp[4], mac_temp[5]);

    // Create MQTT topic string
    node_topic = (char*)malloc(strlen(TOPIC_BASE) + strlen(mac_address));
    strcpy(node_topic, TOPIC_BASE);
    strcat(node_topic, mac_address);


    // Update RTC if device has had a full reset
    if (network_connect())
    {
        if (EEPROM.read(0) == 0)
        {
            ntpClient.begin();
            while (ntpClient.forceUpdate() == false)
            ntpClient.end();

            //rtc.adjust(DateTime(__DATE__, __TIME__));
            //EEPROM.write(0, 1);
        }
    }

    sample_sensors();
}

void sample_sensors()
{
    time_t epoch = ntpClient.getEpochTime();
    tm* date_time = gmtime(&epoch);
    char time[32];
    strftime(time, 32, "%Y-%m-%d %H:%M:%S", date_time);
    Serial.println(time); 

    float airt = 13.3;
    float relh = 93.6;
    int lvis = 88;
    int lifr = 72;
    float batv = 2.54;
    int sigs = WiFi.RSSI();

    sprintf(report, report_base, mac_address, 
        time, airt, relh, lvis, lifr, batv, sigs);
    Serial.println(report);

    if (network_connect() && broker_connect())
        mqtt_client.publish(node_topic, report);
}

bool network_connect()
{
    if (WiFi.status() == WL_CONNECTED) return true;

    int checks = 0;
    WiFi.begin(NETWORK_ID, NETWORK_PASS);
    delay(500);

    // Check status every half second for 8 seconds
    while (true)
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            if (checks++ < 15)
                delay(500);
            else
            {
                WiFi.disconnect();
                return false;
            }
        } else return true;
    }
}

bool broker_connect()
{
    mqtt_client.setServer(BROKER_ADDR, BROKER_PORT);
    mqtt_client.connect(node_topic);
    return mqtt_client.connected();
}


void loop()
{
    // DateTime now = rtc.now();
   
    // if (now.second() == 0)
    // {

    // }
}