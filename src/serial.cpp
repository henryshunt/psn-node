/**
 * Deals with communicating with and configuring the device over serial.
 */

#include "serial.h"
#include "utilities/utilities.h"
#include "main.h"
#include <Preferences.h>
#include <ArduinoJson.h>


/**
 * The received serial command.
 */
static char command[SERIAL_COMMAND_LENGTH + 1] = { '\0' };

/**
 * The index within the array at which to place the next character of the command.
 */
static int cmdPosition = 0;

/**
 * Is the received command longer than the array length allows?
 */
static bool cmdOverflow = false;


static void serialRoutine();
static void commandPing();
static void commandReadConfig();
static void commandWriteConfig(const char* const);
static bool parseConfiguration(const char* const, config_t&);
static void commandReadTime();
static void commandWriteTime(const char* const);


void trySerialMode()
{
    Serial.begin(9600);
    bool serialMode = true;

    delay(1000);

    int checks = 1;
    while (!Serial.available())
    {
        if (checks++ >= SERIAL_TIMEOUT)
        {
            serialMode = false;
            break;
        }
        else delay(1000);
    }

    if (serialMode)
        serialRoutine();

    Serial.end();
}

/**
 * Sits in an infinite loop, receiving and responding to any serial commands.
 */
static void serialRoutine()
{
    while (true)
    {
        bool cmdEnded = false;

        if (Serial.available())
        {
            const char newChar = Serial.read();

            if (newChar != '\n')
            {
                if (cmdPosition < SERIAL_COMMAND_LENGTH)
                    command[cmdPosition++] = newChar;
                else cmdOverflow = true;
            }
            else
            {
                command[cmdPosition] = '\0';
                cmdEnded = true;
            }
        }

        if (cmdEnded)
        {
            cmdPosition = 0;

            if (!cmdOverflow)
            {
                if (strcmp(command, "PING") == 0)
                    commandPing();
                else if (strcmp(command, "READ_CONFIG") == 0)
                    commandReadConfig();
                else if (strncmp(command, "WRITE_CONFIG {", 14) == 0)
                    commandWriteConfig(command + 13);
                else if (strcmp(command, "READ_TIME") == 0)
                    commandReadTime();
                else if (strncmp(command, "WRITE_TIME {", 12) == 0)
                    commandWriteTime(command + 11);
                else Serial.write("ERROR\n");
            }
            else
            {
                Serial.write("ERROR\n");
                cmdOverflow = false;
            }
        }
    }
}

/**
 * Processes the PING command.
 */
static void commandPing()
{
    Serial.write("PSN_NODE\n");
}

/**
 * Processes the READ_CONFIG command by sending the device's configuration data in JSON
 * format.
 */
static void commandReadConfig()
{
    const char* format = "{\"madr\":\"%s\",\"nnam\":\"%s\",\"nent\":%s,\"nunm\":"
        "\"%s\",\"npwd\":\"%s\",\"ladr\":\"%s\",\"lprt\":%u}\n";
    
    char response[335] = { '\0' };
    const char* enterprise = config.isEnterprise ? "true" : "false";
    sprintf(response, format, macAddress, config.networkName, enterprise,
        config.networkUsername, config.networkPassword, config.serverAddress,
        config.serverPort);

    Serial.write(response);
}

/**
 * Processes the WRITE_CONFIG command by updating the device's configuration data with
 * the values from a JSON string.
 * @param json A JSON string representing the new configuration data. Must contain values
 * for all configuration attributes.
 */
static void commandWriteConfig(const char* const json)
{
    config_t newConfig;
    if (!parseConfiguration(json, newConfig))
    {
        Serial.write("ERROR\n");
        return;
    }

    Preferences preferences;
    if (preferences.begin("psn", false))
    {
        preferences.putString("nnam", newConfig.networkName);
        preferences.putBool("nent", newConfig.isEnterprise);
        preferences.putString("nunm", newConfig.networkUsername);
        preferences.putString("npwd", newConfig.networkPassword);
        preferences.putString("ladr", newConfig.serverAddress);
        preferences.putUShort("lprt", newConfig.serverPort);
        preferences.end();

        config = newConfig;
        Serial.write("OK\n");
    }
    else Serial.write("ERROR\n");
}

/**
 * Parses a JSON string representing device configuration data.
 * @param json A JSON string representing the new configuration data. Must contain values
 * for all configuration attributes.
 * @param configOut The destination to place the parsed configuration data in.
 */
static bool parseConfiguration(const char* const json, config_t& configOut)
{
    StaticJsonDocument<JSON_OBJECT_SIZE(30)> document;
    if (deserializeJson(document, json) != DeserializationError::Ok)
        return false;

    JsonObject jsonObject = document.as<JsonObject>();

    // Network name
    if (jsonObject.containsKey("nnam"))
    {
        JsonVariant value = jsonObject.getMember("nnam");

        if (value.is<char*>() && strlen(value) > 0 && strlen(value) <= 31)
            strcpy(configOut.networkName, value);
        else return false;
    }
    else return false;

    // Is enterprise network?
    if (jsonObject.containsKey("nent"))
    {
        JsonVariant value = jsonObject.getMember("nent");

        if (value.is<bool>())
            configOut.isEnterprise = value;
        else return false;
    }
    else return false;

    // Network username
    if (jsonObject.containsKey("nunm"))
    {
        JsonVariant value = jsonObject.getMember("nunm");

        if (value.is<char*>() && strlen(value) <= 63)
            strcpy(configOut.networkUsername, value);
        else return false;
    }
    else return false;

    // Network password
    if (jsonObject.containsKey("npwd"))
    {
        JsonVariant value = jsonObject.getMember("npwd");

        if (value.is<char*>() && strlen(value) <= 63)
            strcpy(configOut.networkPassword, value);
        else return false;
    }
    else return false;

    // Server address
    if (jsonObject.containsKey("ladr"))
    {
        JsonVariant value = jsonObject.getMember("ladr");

        if (value.is<char*>() && strlen(value) > 0 && strlen(value) <= 31)
            strcpy(configOut.serverAddress, value);
        else return false;
    }
    else return false;

    // Server port
    if (jsonObject.containsKey("lprt"))
    {
        JsonVariant value = jsonObject.getMember("lprt");

        if (value.is<uint16_t>() && value >= 1024)
            configOut.serverPort = value;
        else return false;
    }
    else return false;

    if (configOut.isEnterprise &&
        (strlen(configOut.networkUsername) == 0 ||
        strlen(configOut.networkPassword) == 0))
    {
        return false;
    }

    return true;
}

/**
 * Processes the READ_TIME command by sending the time on the RTC in JSON format.
 */
static void commandReadTime()
{
    const RtcDateTime now = ds3231.GetDateTime();
    const bool timeValid = ds3231.IsDateTimeValid();

    if (!ds3231.LastError())
    {
        const char* format = "{\"time\":\"%s\",\"tvld\":%s}\n";

        char time[20] = { '\0' };
        formatTime(now, time);

        char response[100] = { '\0' };
        sprintf(response, format, time, timeValid ? "true" : "false");
        Serial.write(response);
    }
    else Serial.write("ERROR\n");
}

/**
 * Processes the WRITE_TIME command by setting the time on the RTC.
 * @param json A JSON string containing the new time, formatted as the number of seconds
 * since January 1st 2000.
 */
static void commandWriteTime(const char* const json)
{
    StaticJsonDocument<JSON_OBJECT_SIZE(2)> document;
    if (deserializeJson(document, json) != DeserializationError::Ok)
    {
        Serial.write("ERROR\n");
        return;
    }

    JsonObject jsonObject = document.as<JsonObject>();

    if (jsonObject.containsKey("time"))
    {
        JsonVariant value = jsonObject.getMember("time");

        if (value.is<uint32_t>())
        {
            ds3231.SetDateTime(RtcDateTime((uint32_t)value));
            Serial.write(ds3231.LastError() ? "ERROR\n" : "OK\n");
        }
        else Serial.write("ERROR\n");
    }
    else Serial.write("ERROR\n");
}