/*
    Deals with connecting to the WiFi network, and connecting to and communicating
    with the logging server.
 */

#include <WiFi.h>
#include <esp_wpa2.h>
#include <AsyncMqttClient.h>
#include <ArduinoJson.h>

#include "transmit.h"
#include "helpers/globals.h"
#include "helpers/helpers.h"


bool awaiting_subscribe = false;
uint16_t subscribe_id;

uint16_t publish_id = -1;
bool awaiting_session = false;
RequestResult session_result;
session_t new_session;
bool awaiting_report = false;
RequestResult report_result;

AsyncMqttClient logger;


/*
    Connects to the WiFi network or times out (blocking). Returns a boolean
    indicating success or failure.

    NOTE: I cannot guarantee that this function will work properly when called
    multiple times. The only way to ensure the system is not left in an
    unrecoverable state is to perform a restart before calling this again.
 */
bool network_connect()
{
    // Configure for enterprise WiFi network if required
    if (is_enterprise_network)
    {
        WiFi.mode(WIFI_STA);
        esp_wifi_sta_wpa2_ent_set_username(
            (uint8_t *)network_username, strlen(network_username));
        esp_wifi_sta_wpa2_ent_set_password(
            (uint8_t *)network_password, strlen(network_password));
        esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT();
        esp_wifi_sta_wpa2_ent_enable(&config);
    }

    WiFi.begin(network_name, network_password);
    delay(1000);

    // Check connection status and time out after set time
    int checks = 1;
    while (WiFi.status() != WL_CONNECTED)
    {
        if (checks++ >= network_timeout)
            return false;
        else delay(1000);
    }

    return true;
}

/*
    Returns a boolean indicating whether the device is currently connected to
    the network or not.
 */
bool is_network_connected()
{
    return WiFi.status() == WL_CONNECTED;
}


/*
    Connects to the logging server or times out (blocking). Returns a boolean
    indicating success or failure.

    NOTE: I cannot guarantee that this function will work properly when called
    multiple times. The only way to ensure the system is not left in an
    unrecoverable state is to perform a restart before calling this again.
 */
bool logger_connect()
{
    if (WiFi.status() != WL_CONNECTED) return false;

    logger.onSubscribe(logger_on_subscribe);
    logger.onMessage(logger_on_message);
    logger.setServer(logger_address, logger_port);
    logger.connect();
    delay(1000);

    // Check connection status and time out after set time
    int checks = 1;
    while (!logger.connected())
    {
        if (checks++ >= logger_timeout)
            return false;
        else delay(1000);
    }

    return true;
}

/*
    Returns a boolean indicating whether the device is currently connected to
    the logging server or not.
 */
bool is_logger_connected()
{
    return logger.connected();
}


/*
    Subscribes to the inbound topic on the logging server, then waits for
    response or times out (blocking). Returns a boolean indicating success or
    failure.
 */
bool logger_subscribe()
{
    char inbound_topic[64] = { '\0' };
    sprintf(inbound_topic, "nodes/%s/inbound/#", mac_address);

    uint16_t packet_id = logger.subscribe(inbound_topic, 0);

    // Check if successfully sent message
    if (packet_id)
    {
        subscribe_id = packet_id;
        awaiting_subscribe = true;
    } else return false;
    delay(1000);

    // Check result status and time out after set time
    int checks = 1;
    while (awaiting_subscribe)
    {
        if (checks++ >= logger_timeout)
        {
            awaiting_subscribe = false;
            return false;
        } else delay(1000);
    }

    return true;
}

/*
    Requests the active session for this sensor node, then waits for response or
    times out (blocking). Returns an enum indicating the status.

    - session_out: the session to fill out upon request success
 */
RequestResult logger_get_session(session_t* session_out)
{
    char outbound_topic[64] = { '\0' };
    sprintf(outbound_topic, "nodes/%s/outbound/%u", mac_address, ++publish_id);

    uint16_t packet_id = logger.publish(outbound_topic, 0, false, "get_session");

    // Check if successfully sent message
    if (!packet_id)
        return RequestResult::Fail;
    else awaiting_session = true;
    delay(1000);

    // Check result status and time out after set time
    int checks = 1;
    while (awaiting_session)
    {
        if (checks++ >= logger_timeout)
        {
            awaiting_session = false;
            return RequestResult::Fail;
        } else delay(1000);
    }

    if (session_result == RequestResult::Success)
        (*session_out) = new_session;
    return session_result;
}

/*
    Transmits a report, then waits for a response or times out (blocking). Returns
    an enum indicating the status.

    - report: the report to transmit in JSON format
 */
RequestResult logger_transmit_report(const char* report)
{
    char reports_topic[64] = { '\0' };
    sprintf(reports_topic, "nodes/%s/reports/%u", mac_address, ++publish_id);

    uint16_t packet_id = logger.publish(reports_topic, 0, false, report);

    // Check if successfully sent message
    if (packet_id)
        awaiting_report = true;
    else return RequestResult::Fail;
    delay(1000);

    // Check result status and time out after set time
    int checks = 1;
    while (awaiting_report)
    {
        if (checks++ >= logger_timeout)
        {
            awaiting_report = false;
            return RequestResult::Fail;
        } else delay(1000);
    }

    return report_result;
}


/*
    Callback for when a subscription acknowledgement is received from the logging
    server (see Async MQTT Client library).
 */
void logger_on_subscribe(uint16_t packet_id, uint8_t qos)
{
    if (awaiting_subscribe && packet_id == subscribe_id)
        awaiting_subscribe = false;
}

/*
    Callback for when message is received from the logging server (see Async MQTT
    client library).
 */
void logger_on_message(char* topic, char* payload,
    AsyncMqttClientMessageProperties properties, size_t length, size_t index,
    size_t total)
{
    // Get ID of received message from the final topic element
    uint16_t message_id = (uint16_t)strtoul(
        topic + std::string(topic).find_last_of('/') + 1, NULL, 10);

    if (message_id != publish_id) return;

    // Copy message into memory to remove unwanted trailing characters
    char* message = (char*)calloc(length + 1, sizeof(char));
    memcpy(message, payload, length);


    // Process the received message
    if (awaiting_session)
    {
        if (strcmp(message, "no_session") == 0)
            session_result = RequestResult::NoSession;
        else if (strcmp(message, "error") == 0)
            session_result = RequestResult::Fail;

        else
        {
            // Deserialise the JSON containing the session
            StaticJsonDocument<JSON_OBJECT_SIZE(3)> document;
            DeserializationError json_status = deserializeJson(document, message);
            
            if (json_status != DeserializationError::Ok)
            {
                session_result = RequestResult::Fail;
                awaiting_session = false;

                free(message);
                return;
            }


            JsonObject json_object = document.as<JsonObject>();

            bool field_error = false;
            session_t temp_session;

            // Check that all values are present in the JSON
            if (json_object.containsKey("session_id"))
            {
                JsonVariant value = json_object.getMember("session_id");

                if (value.is<uint16_t>())
                    temp_session.session_id = value;
                else field_error = true;
            } else field_error = true;

            if (json_object.containsKey("interval"))
            {
                JsonVariant value = json_object.getMember("interval");
                
                if (value.is<uint8_t>())
                    temp_session.interval = value;
                else field_error = true;
            } else field_error = true;

            if (json_object.containsKey("batch_size"))
            {
                JsonVariant value = json_object.getMember("batch_size");
                
                if (value.is<uint8_t>())
                    temp_session.batch_size = value;
                else field_error = true;
            } else field_error = true;


            // Validate the values
            if (!field_error)
            {
                int allowed_intervals[] = ALLOWED_INTERVALS;
                for (int i = 0; i < ALLOWED_INTERVALS_LEN - 1; i++)
                {
                    // If the interval is valid then perform the rest of the checks
                    if (allowed_intervals[i] == temp_session.interval)
                    {
                        if (temp_session.batch_size >= 1 &&
                            temp_session.batch_size <= BUFFER_CAPACITY)
                        {
                            new_session = temp_session;
                            session_result = RequestResult::Success;
                            awaiting_session = false;

                            free(message);
                            return;
                        } else session_result = RequestResult::Fail;
                    }
                }

                session_result = RequestResult::Fail;
            } else session_result = RequestResult::Fail;
        }

        awaiting_session = false;
    }
    else if (awaiting_report)
    {
        if (strcmp(message, "ok") == 0)
            report_result = RequestResult::Success;
        else if (strcmp(message, "no_session") == 0)
            report_result = RequestResult::NoSession;
        else report_result = RequestResult::Fail;

        awaiting_report = false;
    }

    free(message);
}