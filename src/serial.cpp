#include <Preferences.h>

#include <ArduinoJson.h>

#include "serial.h"
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

        // Process the received command
        if (strncmp(command, "psn_pn", 6) == 0)
            process_pn_command();
        else if (strncmp(command, "psn_rc", 6) == 0)
            process_rc_command();
        else if (strncmp(command, "psn_wc", 6) == 0)
            process_wc_command(command);
        else if (strncmp(command, "psn_rt", 6) == 0)
            process_rt_command();
        else if (strncmp(command, "psn_wt", 6) == 0)
            process_wt_command(command);
    }
}


/*
    Processes and responds to the ping command
 */
void process_pn_command()
{
    Serial.write("psn_pn\n");
}

/*
    Processes and responds to the read configuration command
 */
void process_rc_command()
{
    const char* format = "psn_rc { \"madr\": \"%s\", \"nnam\": \"%s\", \"nent\": %s, \"nunm\": "
        "\"%s\", \"npwd\": \"%s\", \"ladr\": \"%s\", \"lprt\": %u, \"tnet\": %u, \"tlog\": %u }\n";
    
    char response[335] = { '\0' };
    sprintf(response, format, mac_address, network_name, is_enterprise_network ? "true" : "false",
        network_username, network_password, logger_address, logger_port, network_timeout,
        logger_timeout);
    Serial.write(response);
}

/*
    Processes and responds to the write configuration command
 */
void process_wc_command(const char* command)
{
    // Check if there's at least the first character of a JSON object
    if (strncmp(command, "psn_wc {", 8) != 0)
    {
        Serial.write("psn_wcf\n");
        return;
    }

    // Deserialise the JSON string containing the new configuration
    StaticJsonDocument<JSON_OBJECT_SIZE(32)> document;
    DeserializationError error = deserializeJson(document, command + 7);
    
    if (error)
    {
        Serial.write("psn_wcf\n");
        return;
    }

    JsonObject json_object = document.as<JsonObject>();
    bool field_error = false;

    char new_network_name[32] = { '\0' };
    bool new_is_enterprise_network = false;
    char new_network_username[64] = { '\0' };
    char new_network_password[64] = { '\0' };
    char new_logger_address[32] = { '\0' };
    uint16_t new_logger_port = 0;
    uint8_t new_network_timeout = 0;
    uint8_t new_logger_timeout = 0;


    if (json_object.containsKey("nnam"))
    {
        JsonVariant value = json_object.getMember("nnam");
        if (value.is<char*>())
        {
            if (strlen(value) > 0 && strlen(value) <= 31)
                strcpy(new_network_name, value);
            else field_error = true;
        } else field_error = true;
    } else field_error = true;

    if (json_object.containsKey("nent"))
    {
        JsonVariant value = json_object.getMember("nent");
        if (value.is<bool>())
            new_is_enterprise_network = value;
        else field_error = true;
    } else field_error = true;

    if (json_object.containsKey("nunm"))
    {
        JsonVariant value = json_object.getMember("nunm");
        if (value.is<char*>())
        {
            if (strlen(value) <= 63)
                strcpy(new_network_username, value);
            else field_error = true;
        } else field_error = true;
    } else field_error = true;

    if (json_object.containsKey("npwd"))
    {
        JsonVariant value = json_object.getMember("npwd");
        if (value.is<char*>())
        {
            if (strlen(value) <= 63)
                strcpy(new_network_password, value);
            else field_error = true;
        } else field_error = true;
    } else field_error = true;

    if (json_object.containsKey("ladr"))
    {
        JsonVariant value = json_object.getMember("ladr");
        if (value.is<char*>())
        {
            if (strlen(value) > 0 && strlen(value) <= 31)
                strcpy(new_logger_address, value);
            else field_error = true;
        } else field_error = true;
    } else field_error = true;

    if (json_object.containsKey("lprt"))
    {
        JsonVariant value = json_object.getMember("lprt");
        if (value.is<uint16_t>())
            new_logger_port = value;
        else field_error = true;
    } else field_error = true;

    if (json_object.containsKey("tnet"))
    {
        JsonVariant value = json_object.getMember("tnet");
        if (value.is<uint8_t>())
            new_network_timeout = value;
        else field_error = true;
    } else field_error = true;

    if (json_object.containsKey("tlog"))
    {
        JsonVariant value = json_object.getMember("tlog");
        if (value.is<uint8_t>())
            new_logger_timeout = value;
        else field_error = true;
    } else field_error = true;


    if (field_error)
    {
        Serial.write("psn_wcf\n");
        return;
    }

    // Check validity of new configuration (some 0 length checks already done above)
    if (new_is_enterprise_network && (strlen(new_network_username) == 0 ||
        strlen(new_network_password) == 0)) field_error = false;
    if (new_logger_port < 1024) field_error = false;
    if (new_network_timeout < 1 || new_network_timeout > 13) field_error = false;
    if (new_logger_timeout < 1 || new_logger_timeout > 13) field_error = false;

    if (field_error)
    {
        Serial.write("psn_wcf\n");
        return;
    }


    // Write the new configuration to non-volatile storage
    Preferences preferences;
    if (!preferences.begin("psn", false))
    {
        Serial.write("psn_wcf\n");
        return;
    }

    preferences.putString("nnam", new_network_name);
    preferences.putBool("nent", new_is_enterprise_network);
    preferences.putString("nunm", new_network_username);
    preferences.putString("npwd", new_network_password);
    preferences.putString("ladr", new_logger_address);
    preferences.putUShort("lprt", new_logger_port);
    preferences.putUChar("tnet", new_network_timeout);
    preferences.putUChar("tlog", new_logger_timeout);
    preferences.end();

    Serial.write("psn_wcs\n");
}

/*
    Processes and responds to the read time command
 */
void process_rt_command()
{
    RtcDateTime now = rtc.GetDateTime();
    if (!rtc.LastError())
    {
        bool is_time_valid = rtc.IsDateTimeValid();
        if (!rtc.LastError())
        {
            const char* format = "psn_rt { \"time\": \"%s\", \"tvld\": %s }\n";

            char formatted_time[21] = { '\0' };
            format_time(formatted_time, now);

            char response[335] = { '\0' };
            sprintf(response, format, formatted_time, is_time_valid ? "true" : "false");
            Serial.write(response);
        } else Serial.write("psn_rtf\n");
    } else Serial.write("psn_rtf\n");
}

/*
    Processes and responds to the write time command
 */
void process_wt_command(const char* command)
{
    // Check if there's at least the first character of a JSON object
    if (strncmp(command, "psn_wt {", 8) != 0)
    {
        Serial.write("psn_wtf\n");
        return;
    }

    // Deserialise the JSON string containing the new configuration
    StaticJsonDocument<JSON_OBJECT_SIZE(32)> document;
    DeserializationError error = deserializeJson(document, command + 7);
    
    if (error)
    {
        Serial.write("psn_wtf\n");
        return;
    }

    JsonObject json_object = document.as<JsonObject>();

    // If the JSON contains a valid time value then set the RTC time to it
    if (json_object.containsKey("time"))
    {
        JsonVariant value = json_object.getMember("time");
        if (value.is<uint32_t>())
        {
            rtc.SetDateTime(RtcDateTime((uint32_t)value));
            Serial.write(rtc.LastError() ? "psn_wtf\n" : "psn_wts\n");
        } else Serial.write("psn_wtf\n");
    } else Serial.write("psn_wtf\n");
}