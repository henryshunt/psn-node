#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

const char* NETWORK_ID = "***REMOVED***";
const char* NETWORK_PASS = "***REMOVED***";
const char* BROKER_ADDR = "192.168.0.61";
const int BROKER_PORT = 1883;
const char* TOPIC_BASE = "nodes/";

char mac_address[18];
char* node_topic;

WiFiClient network;
PubSubClient mqtt_client(network);


bool network_connect();
bool broker_connect();

void setup()
{
    Serial.begin(9600);
    
    // Get MAC address to identify node
    uint8_t mac_temp[6];
    esp_efuse_mac_get_default(mac_temp);

    sprintf(mac_address, "%X-%X-%X-%X-%X-%X", mac_temp[0],
        mac_temp[1], mac_temp[2], mac_temp[3], mac_temp[4], mac_temp[5]);

    // Create MQTT topic
    node_topic = (char*)malloc(strlen(TOPIC_BASE) + strlen(mac_address));
    strcpy(node_topic, TOPIC_BASE);
    strcat(node_topic, mac_address);


    if (network_connect())
    {
        if (broker_connect())
        {
            Serial.println("ONLINE");
            mqtt_client.publish(node_topic, "test");
        }
    }
}

void sample_sensors()
{

}

bool network_connect()
{
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

void loop() {
    
}