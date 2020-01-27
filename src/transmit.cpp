#include <WiFi.h>
#include <esp_wpa2.h>

#include "AsyncMqttClient.h"
#include "ArduinoJson.h"

#include "transmit.h"
#include "globals.h"


/*
    Connects to WiFi network or times out (blocking)
 */
bool network_connect()
{
    // Configure for enterprise network if required
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
        if (checks++ >= NETWORK_TIMEOUT)
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
        if (checks++ >= LOGGER_TIMEOUT)
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

    // Check if successfully sent message
    if (result)
    {
        subscribe_id = result;
        awaiting_subscribe = true;
    } else return false;
    delay(1000);

    // Check result status and timeout after set time
    int checks = 1;
    while (awaiting_subscribe)
    {
        if (checks++ >= LOGGER_TIMEOUT)
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
    char outbound_topic[64] = { '\0' };
    sprintf(outbound_topic, "nodes/%s/outbound/%u", mac_address, ++publish_id);

    uint16_t result = logger.publish(outbound_topic, 1, false, "get_session");

    // Check if successfully sent message
    if (result)
        awaiting_session = true;
    else return false;
    delay(1000);

    // Check result status and timeout after set time
    int checks = 1;
    while (awaiting_session)
    {
        if (checks++ >= LOGGER_TIMEOUT)
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
    char reports_topic[64] = { '\0' };
    sprintf(reports_topic, "nodes/%s/reports/%u", mac_address, ++publish_id);

    uint16_t result = logger.publish(reports_topic, 0, false, report);

    // Check if successfully sent message
    if (!result)
    {
        report_result = ReportResult::None;
        return false;
    } else awaiting_report = true;
    delay(1000);

    // Check result status and timeout after set time
    int checks = 1;
    while (awaiting_report)
    {
        if (checks++ >= LOGGER_TIMEOUT)
        {
            report_result = ReportResult::None;
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
    if (awaiting_subscribe && packet_id == subscribe_id)
        awaiting_subscribe = false;
}

/*
    Called when the MQTT broker receives a message
 */
void logger_on_message(char* topic, char* payload,
    AsyncMqttClientMessageProperties properties, size_t length, size_t index,
    size_t total)
{
    // Get ID of received message from the final topic element
    uint32_t message_id = strtoul(
        topic + std::string(topic).find_last_of('/') + 1, NULL, 10);

    if (message_id != publish_id) return;

    // Copy message into memory to remove unwanted trailing characters
    char* message = (char*)calloc(length + 1, sizeof(char));
    memcpy(message, payload, length);


    // Process the received message
    if (awaiting_session)
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
    else if (awaiting_report)
    {
        if (strcmp(message, "ok") == 0)
        {
            report_result = ReportResult::Ok;
            awaiting_report = false;
        }
        else if (strcmp(message, "no_session") == 0)
        {
            report_result = ReportResult::NoSession;
            awaiting_report = false;
        }
    }
}