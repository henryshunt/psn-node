/**
 * Contains functions for connecting to the WiFi network, connecting to the server and
 * communicating with the server.
 */

#include "transmit.h"
#include "utilities/utilities.h"
#include "main.h"
#include <WiFi.h>
#include <esp_wpa2.h>
#include <AsyncMqttClient.h>
#include <ArduinoJson.h>


/**
 * The MQTT server.
 */
static AsyncMqttClient server;

/**
 * Stores which server-related asynchronous action is currently being waited on.
 */
static ServerAction awaiting = ServerAction::None;

/**
 * When subscribing to an MQTT topic on the server, stores the ID of the packet that was
 * sent.
 */
static uint16_t subscribeId;

/**
 * When sending a message to the server, stores the ID of the message.
 */
static RTC_DATA_ATTR uint8_t messageId = 0;

/**
 * When sending a message to the server, indicates whether there was an error.
 */
static bool messageError = false;

/**
 * When sending a message to the server that returns instructions, stores the returned
 * instructions.
 */
static instructions_t tempInstructions;


static void onMqttSubscribe(uint16_t, uint8_t);
static void onMqttMessage(char*, char*, AsyncMqttClientMessageProperties,
    size_t, size_t, size_t);
static bool parseInstructions(const char* const, instructions_t&);


bool networkConnect()
{
    if (cfgIsEnterprise)
    {
        WiFi.mode(WIFI_STA);
        
        esp_wifi_sta_wpa2_ent_set_username(
            (uint8_t *)cfgNetworkUsername, strlen(cfgNetworkUsername));
        esp_wifi_sta_wpa2_ent_set_password(
            (uint8_t *)cfgNetworkPassword, strlen(cfgNetworkPassword));

        esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT();
        esp_wifi_sta_wpa2_ent_enable(&config);
    }

    WiFi.begin(cfgNetworkName, cfgNetworkPassword);
    delay(1000);

    int checks = 1;
    while (WiFi.status() != WL_CONNECTED)
    {
        if (checks++ >= NETWORK_TIMEOUT)
            return false;
        else delay(1000);
    }

    return true;
}

bool serverConnect()
{
    if (WiFi.status() != WL_CONNECTED)
        return false;

    server.onSubscribe(onMqttSubscribe);
    server.onMessage(onMqttMessage);
    server.setServer(cfgServerAddress, cfgServerPort);

    server.connect();
    delay(1000);

    int checks = 1;
    while (!server.connected())
    {
        if (checks++ >= NETWORK_TIMEOUT)
            return false;
        else delay(1000);
    }

    return true;
}

bool serverSubscribe()
{
    char topic[36] = { '\0' };
    sprintf(topic, "psn/nodes/%s/inbound", macAddress);

    subscribeId = server.subscribe(topic, 0);

    if (subscribeId)
        awaiting = ServerAction::Subscribe;
    else return false;

    delay(1000);

    int checks = 1;
    while (awaiting == ServerAction::Subscribe)
    {
        if (checks++ >= NETWORK_TIMEOUT)
        {
            awaiting = ServerAction::None;
            return false;
        }
        else delay(1000);
    }

    return true;
}

/**
 * Callback for when a subscription acknowledgement is received from the MQTT server.
*/
static void onMqttSubscribe(uint16_t packetId, uint8_t)
{
    if (awaiting == ServerAction::Subscribe && packetId == subscribeId)
        awaiting = ServerAction::None;
}

bool serverInstructions(instructions_t& instrucOut)
{
    char topic[37] = { '\0' };
    sprintf(topic, "psn/nodes/%s/outbound", macAddress);

    char message[17] = { '\0' };
    sprintf(message, "%d INSTRUCTIONS", ++messageId);

    if (server.publish(topic, 0, false, message))
    {
        messageError = false;
        awaiting = ServerAction::Instructions;
    }
    else return false;

    delay(1000);

    int checks = 1;
    while (awaiting == ServerAction::Instructions)
    {
        if (checks++ >= NETWORK_TIMEOUT)
        {
            awaiting = ServerAction::None;
            return false;
        }
        else delay(1000);
    }

    if (!messageError)
        instrucOut = tempInstructions;

    return !messageError;
}

bool serverObservation(const char* const obsJson, instructions_t& instrucOut)
{
    char topic[37] = { '\0' };
    sprintf(topic, "psn/nodes/%s/outbound", macAddress);

    char message[113] = { '\0' };
    sprintf(message, "%d OBSERVATION ", ++messageId);
    strcat(message, obsJson);

    if (server.publish(topic, 0, false, message))
    {
        messageError = false;
        awaiting = ServerAction::Observation;
    }
    else return false;

    delay(1000);

    int checks = 1;
    while (awaiting == ServerAction::Observation)
    {
        if (checks++ >= NETWORK_TIMEOUT)
        {
            awaiting = ServerAction::None;
            return false;
        }
        else delay(1000);
    }

    if (!messageError)
        instrucOut = tempInstructions;

    return !messageError;
}

/**
 * Callback for when a message is received from the MQTT server.
 */
static void onMqttMessage(char*, char* payload,
    AsyncMqttClientMessageProperties, size_t length, size_t, size_t)
{
    if (awaiting == ServerAction::None)
        return;

    // Need to remove unwanted trailing characters
    char* message = (char*)calloc(length + 1, sizeof(char));
    memcpy(message, payload, length);

    char* spacePtr = strchr(message, ' ');
    int spaceIndex = spacePtr == NULL ? -1 : spacePtr - message;

    if (spaceIndex <= 0 || spaceIndex == length - 1 ||
        message[spaceIndex + 1] == ' ' || message[length - 1] == ' ')
    {
        return;
    }

    char* endPtr = NULL;
    unsigned long receivedId = strtoul(message, &endPtr, 10);

    if (endPtr != spacePtr || receivedId > 255 || receivedId != messageId)
        return;

    if (strncmp(spacePtr + 1, "ERROR", 5) == 0)
        messageError = true;
    else
    {
        // Both currently supported messages return instructions
        if (strncmp(spacePtr + 1, "NULL", 4) != 0)
        {
            messageError = !parseInstructions(spacePtr + 1, tempInstructions);
            tempInstructions.isNull = false;
        }
        else tempInstructions.isNull = true;
    }

    free(message);
    awaiting = ServerAction::None;
}

/**
 * Parses a JSON string representing the instructions for a sensor node.
 * @param json The JSON string.
 * @param instrucOut The destination for the parsed instructions.
 * @return An indication of success or failure.
 */
static bool parseInstructions(const char* const json, instructions_t& instrucOut)
{
    StaticJsonDocument<JSON_OBJECT_SIZE(5)> document;
    DeserializationError status = deserializeJson(document, json);
    
    if (status != DeserializationError::Ok)
        return false;

    JsonObject jsonObject = document.as<JsonObject>();


    if (jsonObject.containsKey("streamId"))
    {
        JsonVariant value = jsonObject.getMember("streamId");

        if (value.is<uint16_t>())
            instrucOut.streamId = value;
        else return false;
    }
    else return false;

    if (jsonObject.containsKey("interval"))
    {
        JsonVariant value = jsonObject.getMember("interval");
        
        if (value.is<uint8_t>())
            instrucOut.interval = value;
        else return false;
    }
    else return false;

    if (jsonObject.containsKey("batchSize"))
    {
        JsonVariant value = jsonObject.getMember("batchSize");
        
        if (value.is<uint8_t>())
            instrucOut.batchSize = value;
        else return false;
    }
    else return false;


    int allowedIntervals[] = ALLOWED_INTERVALS;

    for (int i = 0; i < ALLOWED_INTERVALS_LEN - 1; i++)
    {
        if (allowedIntervals[i] == instrucOut.interval)
        {
            return instrucOut.batchSize >= 1 &&
                instrucOut.batchSize <= BUFFER_CAPACITY;
        }
    }

    return false;
}