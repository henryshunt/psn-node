#include <nvs_flash.h>
#include <Preferences.h>
#include <WiFi.h>

#include "RtcDS3231.h"
#include "Adafruit_Sensor.h"
#include "Adafruit_BME680.h"
#include "ArduinoJson.h"

#include "main.h"
#include "globals.h"
#include "transmit.h"
#include "helpers/helpers.h"
#include "helpers/buffer.h"


void setup()
{
    Serial.begin(9600);
    rtc.Begin();

    if (cold_boot)
    {
        // Load device MAC address for unique node identification
        uint8_t mac_out[6];
        esp_efuse_mac_get_default(mac_out);

        sprintf(mac_address, "%x:%x:%x:%x:%x:%x", mac_out[0], mac_out[1],
            mac_out[2], mac_out[3], mac_out[4], mac_out[5]);

        // Load device configuration from non-volatile storage
        if (!config.begin("psn", false)) esp_deep_sleep_start();
            
        bool config_result = load_configuration();


        // Permanently enter serial mode if serial data received before timeout
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


        // Check configuration is valid
        if (!config_result) esp_deep_sleep_start();

        // Check RTC time is valid (may not be set or may have lost power)
        bool rtc_valid = rtc.IsDateTimeValid();
        if (!rtc.LastError())
        {
            if (!rtc_valid)
                esp_deep_sleep_start();
        } else esp_deep_sleep_start();


        // Connect to network and logging server and reboot on failure
        // NOTE: I cannot fully guarantee these functions will work properly
        // when called multiple times. The only way to ensure the system is not
        // left in an unrecoverable state is to perform a full restart.
        if (!network_connect() || !logger_connect()) esp_restart();

        // Subscribe to inbound topic on logging server
        while (true)
        {
            if (!logger_subscribe())
            {
                if (WiFi.status() != WL_CONNECTED || !logger.connected())
                    esp_restart();
            } else break;
        }

        // Request active session for this node
        while (true)
        {
            if (!logger_session())
            {
                if (WiFi.status() != WL_CONNECTED || !logger.connected())
                    esp_restart();
            } else break;
        }

        // No active session for this node
        if (session_id == -1) esp_deep_sleep_start();


        buffer.maximum_size = BUFFER_MAX;
        rtc.SetSquareWavePin(DS3231SquareWavePin_ModeAlarmOne);

        RtcDateTime first_alarm = rtc.GetDateTime();
        first_alarm += (60 - first_alarm.Second()); // Start of next minute
        first_alarm = round_up(first_alarm, session_interval * 60); // Round up
        // to next multiple of the interval (so e.g. a 5 minute interval always
        // triggers on minutes ending in 0 and 5)

        // Advance to next interval if too close to first available interval
        if (first_alarm - rtc.GetDateTime() <= ALARM_SET_THRESHOLD)
            first_alarm += session_interval * 60;

        // Set alarm for first report then go into deep sleep
        set_alarm(rtc, first_alarm);
        esp_sleep_enable_ext0_wakeup(RTC_SQUARE_WAVE_PIN, 0);
        cold_boot = false;
        esp_deep_sleep_start();
    }
    else wake_routine();
}

void loop() { }


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
                char read_char = Serial.read();
                if (read_char != '\n')
                    command[position++] = read_char;
                else line_ended = true;
            }
        }
        
        // Respond to ping command command
        if (strncmp(command, "psna_pn", 7) == 0)
            Serial.write("psna_pnr\n");
        
        // Respond to read configuration command
        else if (strncmp(command, "psna_rc", 7) == 0)
        {
            char config[200] = { '\0' };
            int length = 0;
    
            length += sprintf(config, "psna_rcr { \"madr\": \"");
            length += sprintf(config + length, mac_address);
            length += sprintf(
                config + length, "\", \"nent\": %d", NETWORK_ENTERPRISE);

            if (NETWORK_NAME[0] != '\0')
            {
                length += sprintf(
                    config + length, ", \"nnam\": \"%s\"", NETWORK_NAME);
            } else length += sprintf(config + length, ", \"nnam\": null");

            if (NETWORK_USERNAME[0] != '\0')
            {
                length += sprintf(
                    config + length, ", \"nunm\": \"%s\"", NETWORK_USERNAME);
            } else length += sprintf(config + length, ", \"nunm\": null");

            if (NETWORK_PASSWORD[0] != '\0')
            {
                length += sprintf(
                    config + length, ", \"npwd\": \"%s\"", NETWORK_PASSWORD);
            } else length += sprintf(config + length, ", \"npwd\": null");

            if (LOGGER_ADDRESS[0] != '\0')
            {
                length += sprintf(
                    config + length, ", \"ladr\": \"%s\"", LOGGER_ADDRESS);
            } else length += sprintf(config + length, ", \"ladr\": null");

            length += sprintf(config + length, ", \"lprt\": %u", LOGGER_PORT);
            length += sprintf(
                config + length, ", \"tnet\": %u", NETWORK_TIMEOUT);
            length += sprintf(
                config + length, ", \"tlog\": %u", LOGGER_TIMEOUT);
            strcat(config + length, " }\n");
            
            Serial.write(config);
        }

        // Respond to write configuration command
        else if (strncmp(command, "psna_wc {", 9) == 0)
        {
            StaticJsonDocument<300> document;
            DeserializationError deser = deserializeJson(document, command + 8);
            
            if (deser == DeserializationError::Ok)
            {
                if (!config.begin("psn", false))
                {
                    Serial.write("psna_wcf\n");
                    return;
                }

                if (document.containsKey("nent"))
                    config.putBool("nent", document["nent"]);
                else Serial.write("psna_wcf\n");

                if (document.containsKey("nnam"))
                    config.putString("nnam", (const char*)document["nnam"]);
                else Serial.write("psna_wcf\n");

                if (document.containsKey("nunm"))
                    config.putString("nunm", (const char*)document["nunm"]);
                else Serial.write("psna_wcf\n");

                if (document.containsKey("npwd"))
                    config.putString("npwd", (const char*)document["npwd"]);
                else Serial.write("psna_wcf\n");

                if (document.containsKey("ladr"))
                    config.putString("ladr", (const char*)document["ladr"]);
                else Serial.write("psna_wcf\n");

                if (document.containsKey("lprt"))
                    config.putUShort("lprt", document["lprt"]);
                else Serial.write("psna_wcf\n");

                if (document.containsKey("tnet"))
                    config.putUChar("tnet", document["tnet"]);
                else Serial.write("psna_wcf\n");

                if (document.containsKey("tlog"))
                    config.putUChar("tlog", document["tlog"]);
                else Serial.write("psna_wcf\n");

                config.end();
                Serial.write("psna_wcs\n");
            }
            else Serial.write("psna_wcf\n");
        }
    }
}

/*
    Generates a report, sets next alarm, transmits reports, goes to sleep
 */
void wake_routine()
{
    // Check if RTC time is valid (may have lost power)
    if (!rtc_time_valid(rtc)) esp_deep_sleep_start();
    
    // Set alarm for next report
    RtcDateTime now = rtc.GetDateTime();
    RtcDateTime next_alarm = now + (session_interval * 60);
    set_alarm(rtc, next_alarm);


    generate_report(now);

    // Transmit reports in buffer (only if there's enough reports)
    if (buffer.count >= session_batch_size && network_connect()
        && logger_connect() && logger_subscribe())
    {
        // Only transmit if there's enough time before next alarm
        while (!buffer.is_empty() && next_alarm - now >= LOGGER_TIMEOUT
            + ALARM_SET_THRESHOLD)
        {
            report_t report = buffer.pop_rear(reports);
            char report_json[128] = { '\0' };
            report_to_string(report_json, report, session_id);
            Serial.println(report_json);

            if (!logger_report(report_json, report.time))
            {
                buffer.push_rear(reports, report);
                break;
            }
            else
            {
                // Session has ended so go to sleep permanently
                if (report_result == ReportResult::NoSession)
                    esp_deep_sleep_start();
            }
        }
    }

    // Go into deep sleep
    esp_sleep_enable_ext0_wakeup(RTC_SQUARE_WAVE_PIN, 0);
    esp_deep_sleep_start();
}

/*
    Samples the sensors, generates a report and adds it to the buffer
 */
void generate_report(const RtcDateTime& time)
{
    report_t report = { time, -99, -99, -99 };

    // Sample temperature and humidity
    Adafruit_BME680 bme680;
    if (bme680.begin(0x76))
    {
        bme680.setTemperatureOversampling(BME680_OS_8X);
        bme680.setHumidityOversampling(BME680_OS_2X);

        if (bme680.performReading())
        {
            report.airt = bme680.temperature;
            report.relh = bme680.humidity;
        }
    }

    // Sample battery voltage
    // report.batv = ...

    buffer.push_front(reports, report);
}


/*
    Loads configuration from NVS into global variables and checks validity
 */
bool load_configuration()
{
    NETWORK_ENTERPRISE = config.getBool("nent", false);
    strcpy(NETWORK_NAME, config.getString("nnam", String()).c_str());
    strcpy(NETWORK_USERNAME, config.getString("nunm", String()).c_str());
    strcpy(NETWORK_PASSWORD, config.getString("npwd", String()).c_str());
    strcpy(LOGGER_ADDRESS, config.getString("ladr", String()).c_str());
    LOGGER_PORT = config.getUShort("lprt", 1883);
    NETWORK_TIMEOUT = config.getUChar("tnet", 10);
    LOGGER_TIMEOUT = config.getUChar("tlog", 8);
    config.end();

    if (strlen(NETWORK_NAME) == 0 || strlen(NETWORK_PASSWORD) == 0 ||
            (NETWORK_ENTERPRISE && strlen(NETWORK_USERNAME) == 0) ||
            strlen(LOGGER_ADDRESS) == 0 || NETWORK_TIMEOUT > 13 ||
            LOGGER_TIMEOUT > 13)
        return false;
    else return true;
}