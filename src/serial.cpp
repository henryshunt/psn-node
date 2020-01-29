#include <nvs_flash.h>
#include <Preferences.h>

#include "ArduinoJson.h"

#include "globals.h"
#include "helpers/helpers.h"

/*
    Sits in a continuous loop and responds to commands sent over serial
 */
void serial_routine()
{
    while (true)
    {
        char command[200] = { '\0' };
        int position = 0;
        bool line_ended = false;

        // Store received characters until we receive a new line character
        while (!line_ended)
        {
            while (Serial.available())
            {
                char new_char = Serial.read();
                if (new_char != '\n')
                    command[position++] = new_char;
                else line_ended = true;
            }
        }

        
        // Respond to ping command
        if (strncmp(command, "psn_pn", 6) == 0)
            Serial.write("psn_pn\n");
        
        // Respond to read configuration command
        else if (strncmp(command, "psn_rc", 6) == 0)
        {
            char response[335] = { '\0' };
            int length = 0;
    
            length += sprintf(
                response, "psn_rc { \"madr\": \"%s\"", mac_address);
            length += sprintf(response + length,
                ", \"nent\": %s", NETWORK_ENTERPRISE ? "true" : "false");

            if (NETWORK_NAME[0] != '\0')
            {
                length += sprintf(
                    response + length, ", \"nnam\": \"%s\"", NETWORK_NAME);
            } else length += sprintf(response + length, ", \"nnam\": \"\"");

            if (NETWORK_USERNAME[0] != '\0')
            {
                length += sprintf(
                    response + length, ", \"nunm\": \"%s\"", NETWORK_USERNAME);
            } else length += sprintf(response + length, ", \"nunm\": \"\"");

            if (NETWORK_PASSWORD[0] != '\0')
            {
                length += sprintf(
                    response + length, ", \"npwd\": \"%s\"", NETWORK_PASSWORD);
            } else length += sprintf(response + length, ", \"npwd\": \"\"");

            if (LOGGER_ADDRESS[0] != '\0')
            {
                length += sprintf(
                    response + length, ", \"ladr\": \"%s\"", LOGGER_ADDRESS);
            } else length += sprintf(response + length, ", \"ladr\": \"\"");

            length += sprintf(response + length, ", \"lprt\": %u", LOGGER_PORT);
            length += sprintf(
                response + length, ", \"tnet\": %u", NETWORK_TIMEOUT);
            length += sprintf(
                response + length, ", \"tlog\": %u", LOGGER_TIMEOUT);
            strcat(response + length, " }\n");
            
            Serial.write(response);
        }

        // Respond to write configuration command
        else if (strncmp(command, "psn_wc {", 8) == 0)
        {
            StaticJsonDocument<JSON_OBJECT_SIZE(8)> document;
            DeserializationError error = deserializeJson(document, command + 7);
            
            if (error)
            {
                Serial.write("psn_wcf\n");
                continue;
            }

            JsonObject json_object = document.as<JsonObject>();
            bool field_error = false;

            bool NEW_NETWORK_ENTERPRISE;
            char NEW_NETWORK_NAME[32] = { '\0' };
            char NEW_NETWORK_USERNAME[64] = { '\0' };
            char NEW_NETWORK_PASSWORD[64] = { '\0' };
            char NEW_LOGGER_ADDRESS[32] = { '\0' };
            uint16_t NEW_LOGGER_PORT;
            uint8_t NEW_NETWORK_TIMEOUT;
            uint8_t NEW_LOGGER_TIMEOUT;


            if (json_object.containsKey("nent"))
            {
                JsonVariant value = json_object.getMember("nent");
                if (value.is<bool>())
                    NEW_NETWORK_ENTERPRISE = value;
                else field_error = true;
            } else field_error = true;

            if (json_object.containsKey("nnam"))
            {
                JsonVariant value = json_object.getMember("nnam");
                if (value.is<char*>())
                {
                    if (value[0] != '\0' && strlen(value) <= 31)
                        strcpy(NEW_NETWORK_NAME, value);
                    else field_error = true;
                } else field_error = true;
            } else field_error = true;

            if (json_object.containsKey("nunm"))
            {
                JsonVariant value = json_object.getMember("nunm");
                if (value.is<char*>())
                {
                    if (value[0] != '\0')
                    {
                        if (strlen(value) <= 63)
                            strcpy(NEW_NETWORK_USERNAME, value);
                        else field_error = true;
                    }
                } else field_error = true;
            } else field_error = true;

            if (json_object.containsKey("npwd"))
            {
                JsonVariant value = json_object.getMember("npwd");
                if (value.is<char*>())
                {
                    if (value[0] != '\0')
                    {
                        if (strlen(value) <= 63)
                            strcpy(NEW_NETWORK_PASSWORD, value);
                        else field_error = true;
                    }
                } else field_error = true;
            } else field_error = true;

            if (json_object.containsKey("ladr"))
            {
                JsonVariant value = json_object.getMember("ladr");
                if (value.is<char*>())
                {
                    if (value[0] != '\0' && strlen(value) <= 31)
                        strcpy(NEW_LOGGER_ADDRESS, value);
                    else field_error = true;
                } else field_error = true;
            } else field_error = true;

            if (json_object.containsKey("lprt"))
            {
                JsonVariant value = json_object.getMember("lprt");
                if (value.is<uint16_t>())
                    NEW_LOGGER_TIMEOUT = value;
                else field_error = true;
            } else field_error = true;

            if (json_object.containsKey("tnet"))
            {
                JsonVariant value = json_object.getMember("tnet");
                if (value.is<uint8_t>())
                    NEW_NETWORK_TIMEOUT = value;
                else field_error = true;
            } else field_error = true;

            if (json_object.containsKey("tlog"))
            {
                JsonVariant value = json_object.getMember("tlog");
                if (value.is<uint8_t>())
                    NEW_LOGGER_TIMEOUT = value;
                else field_error = true;
            } else field_error = true;

            if (field_error)
            {
                Serial.write("psn_wcf\n");
                continue;
            }


            // Check validity of new configuration
            if ((NEW_NETWORK_ENTERPRISE && (NEW_NETWORK_USERNAME[0] == '\0' ||
                NEW_NETWORK_PASSWORD[0] == '\0')) || NEW_LOGGER_PORT < 1024 || 
                NEW_NETWORK_TIMEOUT < 1 || NEW_NETWORK_TIMEOUT > 13 || 
                NEW_LOGGER_TIMEOUT < 1 || NEW_LOGGER_TIMEOUT > 13)
            {
                Serial.write("psn_wcf\n");
                continue;
            }

            // Write the new configuration to non-volatile storage
            Preferences preferences;
            if (!preferences.begin("psn", false))
            {
                Serial.write("psn_wcf\n");
                continue;
            }

            preferences.putBool("nent", NEW_NETWORK_ENTERPRISE);
            preferences.putString("nnam", NEW_NETWORK_NAME);
            preferences.putString("nunm", NEW_NETWORK_USERNAME);
            preferences.putString("npwd", NEW_NETWORK_PASSWORD);
            preferences.putString("ladr", NEW_LOGGER_ADDRESS);
            preferences.putUShort("lprt", NEW_LOGGER_PORT);
            preferences.putUChar("tnet", NEW_NETWORK_TIMEOUT);
            preferences.putUChar("tlog", NEW_LOGGER_TIMEOUT);

            preferences.end();
            Serial.write("psn_wcs\n");
        }
    }
}