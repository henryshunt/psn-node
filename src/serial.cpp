/*
    Deals with communication between the device and another computer over the
    serial connection.
 */

#include <Preferences.h>
#include <ArduinoJson.h>

#include "serial.h"
#include "main.h"
#include "utilities/utilities.h"


/*
    Waits a certain amount of time for data to be received on the serial port
    and if it is, goes into an infinite loop to respond to serial commands.
 */
void trySerialMode()
{
    //Serial.begin(9600);
    bool serial_mode = true;
    delay(1000);

    int checks = 1;
    while (!Serial.available())
    {
        if (checks++ >= SERIAL_TIMEOUT)
        {
            serial_mode = false;
            break;
        } else delay(1000);
    }

    if (serial_mode) serial_routine();
    Serial.end();
}

/*
    Sits in an infinite loop and responds to any commands sent over the serial
    connection.
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
    Processes and responds to the ping command.
 */
void process_pn_command()
{
    Serial.write("psn_pn\n");
}

/*
    Processes and responds to the read configuration command. Sends the device
    configuration stored in non-volatile storage, in JSON format.
 */
void process_rc_command()
{
    const char* format = "psn_rc {\"madr\":\"%s\",\"nnam\":\"%s\",\"nent\":%s,\"nunm\":"
        "\"%s\",\"npwd\":\"%s\",\"ladr\":\"%s\",\"lprt\":%u,\"tnet\":%u,\"tlog\":%u}\n";
    
    char response[335] = { '\0' };
    sprintf(response, format, macAddress, cfgNetworkName,
        cfgIsEnterprise ? "true" : "false", cfgNetworkUsername,
        cfgNetworkPassword, cfgServerAddress, cfgServerPort);

    Serial.write(response);
}

/*
    Processes and responds to the write configuration command. Modifies the
    configuration stored in non-volatile storage.

    - command: a JSON string containing new values for all configuration keys
 */
void process_wc_command(const char* command)
{
    // Check if there's at least the first character of a JSON object
    if (strncmp(command, "psn_wc {", 8) != 0)
    {
        Serial.write("psn_wcf\n");
        return;
    }

    // Deserialise the JSON containing the new configuration
    StaticJsonDocument<JSON_OBJECT_SIZE(32)> document;
    DeserializationError json_status = deserializeJson(document, command + 7);
    
    if (json_status != DeserializationError::Ok)
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


    // Check that all values are present in the JSON
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


    if (field_error)
    {
        Serial.write("psn_wcf\n");
        return;
    }

    // Validate the values (some 0 length checks already done above)
    if (new_is_enterprise_network && (strlen(new_network_username) == 0 ||
        strlen(new_network_password) == 0)) field_error = false;
    if (new_logger_port < 1024) field_error = false;

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
    preferences.end();

    Serial.write("psn_wcs\n");
}

/*
    Processes and responds to the read time command. Sends the device time in
    JSON format.
 */
void process_rt_command()
{
    RtcDateTime now = ds3231.GetDateTime();
    if (!ds3231.LastError())
    {
        bool is_time_valid = ds3231.IsDateTimeValid();
        if (!ds3231.LastError())
        {
            const char* format = "psn_rt {\"time\":\"%s\",\"tvld\":%s}\n";

            char formatted_time[21] = { '\0' };
            formatTime(now, formatted_time);

            char response[335] = { '\0' };
            sprintf(response, format, formatted_time, is_time_valid ? "true" : "false");
            Serial.write(response);
        } else Serial.write("psn_rtf\n");
    } else Serial.write("psn_rtf\n");
}

/*
    Processes and responds to the write time command. Sets the device RTC time.

    - command: a JSON string containing a new time value in the form of the
    number of seconds since January 1st 1970
 */
void process_wt_command(const char* command)
{
    // Check if there's at least the first character of a JSON object
    if (strncmp(command, "psn_wt {", 8) != 0)
    {
        Serial.write("psn_wtf\n");
        return;
    }

    // Deserialise the JSON containing the new time
    StaticJsonDocument<JSON_OBJECT_SIZE(32)> document;
    DeserializationError json_status = deserializeJson(document, command + 7);
    
    if (json_status != DeserializationError::Ok)
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
            ds3231.SetDateTime(RtcDateTime((uint32_t)value));
            Serial.write(ds3231.LastError() ? "psn_wtf\n" : "psn_wts\n");
        } else Serial.write("psn_wtf\n");
    } else Serial.write("psn_wtf\n");
}