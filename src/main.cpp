/*
    Deals with device startup as well as the main logging routine responsible for
    generating and transmitting reports. Core program flow is here.
 */

#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

#include "main.h"
#include "helpers/globals.h"
#include "helpers/helpers.h"
#include "helpers/buffer.h"
#include "serial.h"
#include "transmit.h"


RTC_DATA_ATTR bool cold_boot = true;
RTC_DATA_ATTR session_t session;
RTC_DATA_ATTR report_buffer_t buffer;
RTC_DATA_ATTR report_t reports[BUFFER_CAPACITY + 1];


/*
    Performs initialisation, retrieves the active session for the device and sets
    an alarm to trigger the first report.
 */
void setup()
{
    rtc.Begin();

    if (cold_boot)
    {
        // Retrieve device MAC address for uniquely identifying this sensor node
        uint8_t mac_temp[6];
        esp_efuse_mac_get_default(mac_temp);

        sprintf(mac_address, "%x:%x:%x:%x:%x:%x", mac_temp[0], mac_temp[1],
            mac_temp[2], mac_temp[3], mac_temp[4], mac_temp[5]);

        // Load device configuration from non-volatile storage
        bool config_valid;
        if (!load_configuration(&config_valid)) esp_deep_sleep_start();


        // Permanently enter serial mode if serial data is received before timeout
        Serial.begin(9600);

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


        // Check configuration is valid
        if (!config_valid) esp_deep_sleep_start();

        // Check RTC time is valid (may not be set or may have lost battery power)
        if (!is_rtc_time_valid()) esp_deep_sleep_start();


        // Connect to network and logging server, and reboot on failure. NOTE: I
        // cannot fully guarantee these functions will work properly when called
        // multiple times. The only way to ensure the system is not left in an
        // unrecoverable state is to perform a full restart.
        if (!network_connect() || !logger_connect()) esp_restart();

        // Subscribe to the inbound topic on the logging server
        while (true)
        {
            if (!logger_subscribe())
            {
                if (!is_network_connected() || !is_logger_connected())
                    esp_restart();
            } else break;
        }

        // Request the active session for this sensor node
        RequestResult session_status;

        while (true)
        {
            session_status = logger_get_session(&session);
            if (session_status == RequestResult::Fail)
            {
                if (!is_network_connected() || !is_logger_connected())
                    esp_restart();
            } else break;
        }

        // Don't continue if there's no session for this sensor node
        if (session_status == RequestResult::NoSession) esp_deep_sleep_start();


        rtc.SetSquareWavePin(DS3231SquareWavePin_ModeAlarmOne);

        RtcDateTime first_alarm = rtc.GetDateTime();
        first_alarm += (60 - first_alarm.Second()); // Move to start of next minute
        first_alarm = round_up_multiple(first_alarm, session.interval * 60); // Round
        // up to next multiple of the interval (e.g. a 5 minute interval rounds to
        // the next minute ending in 0 or 5)

        // Advance to next interval if currently too close to first available interval
        if (first_alarm - rtc.GetDateTime() <= ALARM_SET_THRESHOLD)
            first_alarm += session.interval * 60;

        // Set alarm to trigger the first report, then go into deep sleep
        set_rtc_alarm(first_alarm);
        esp_sleep_enable_ext0_wakeup(RTC_SQUARE_WAVE_PIN, 0);
        cold_boot = false;
        esp_deep_sleep_start();
    }
    else wake_routine();
}

void loop() { }


/*
    Sets alarm to trigger the next report, generates a report, transmits all
    reports in the buffer, then goes to sleep.
 */
void wake_routine()
{
    Serial.begin(9600);

    // Check if the RTC time is valid (may have lost battery power)
    if (!is_rtc_time_valid()) esp_deep_sleep_start();
    
    // Set alarm to trigger the next report
    RtcDateTime now = rtc.GetDateTime();
    RtcDateTime next_alarm = now + (session.interval * 60);
    set_rtc_alarm(next_alarm);


    generate_report(now);

    // Transmit all reports in the report buffer if there's a enough of them
    if (buffer.count() >= session.batch_size && network_connect() && logger_connect()
        && logger_subscribe())
    {
        // Only transmit if there's enough time before the next alarm
        while (!buffer.is_empty() && next_alarm - rtc.GetDateTime() >= logger_timeout
            + ALARM_SET_THRESHOLD)
        {
            report_t report = buffer.peek_rear(reports);
            char report_json[128] = { '\0' };
            serialise_report(report_json, report);

            // Transmit the report
            RequestResult report_status = logger_transmit_report(report_json);
            if (report_status != RequestResult::Fail)
            {
                buffer.pop_rear(reports);

                // The active session for this sensor node has ended
                if (report_status == RequestResult::NoSession)
                    esp_deep_sleep_start();
            } else break;
        }
    }

    // Go into deep sleep
    esp_sleep_enable_ext0_wakeup(RTC_SQUARE_WAVE_PIN, 0);
    esp_deep_sleep_start();
}

/*
    Samples the sensors, creates a report and pushes it onto the report buffer.

    - time: the time of the report
 */
void generate_report(const RtcDateTime& time)
{
    report_t report = { (uint32_t)time, -99, -99, -99 };

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
    Serialises a report into a JSON string ready for transmission to the logging
    server.

    - report_out: destination string
    - report: the report to serialise
 */
void serialise_report(char* report_out, const report_t& report)
{
    int length = 0;
    length += sprintf(report_out, "{\"session_id\":%d", session.session_id);

    char formatted_time[32] = { '\0' };
    format_time(formatted_time, RtcDateTime(report.time));
    length += sprintf(report_out + length, ",\"time\":\"%s\"", formatted_time);

    if (report.airt != -99)
        length += sprintf(report_out + length, ",\"airt\":%.1f", report.airt);
    else length += sprintf(report_out + length, ",\"airt\":null");

    if (report.relh != -99)
        length += sprintf(report_out + length, ",\"relh\":%.1f", report.relh);
    else length += sprintf(report_out + length, ",\"relh\":null");

    if (report.batv != -99)
        length += sprintf(report_out + length, ",\"batv\":%.2f", report.batv);
    else length += sprintf(report_out + length, ",\"batv\":null");

    strcat(report_out + length, "}");
}


/*
    Returns a boolean indicating whether the RTC holds a valid timestamp or not
    (may not be valid e.g. if the time was never set or onboard battery power
    was lost).
*/
bool is_rtc_time_valid()
{
    bool rtc_valid = rtc.IsDateTimeValid();
    return rtc.LastError() ? false : rtc_valid;
}

/*
    Sets the onboard alarm on the RTC.

    - time: the time that the alarm should trigger at
 */
void set_rtc_alarm(const RtcDateTime& time)
{
    DS3231AlarmOne alarm(time.Day(), time.Hour(), time.Minute(), time.Second(),
        DS3231AlarmOneControl_MinutesSecondsMatch);

    rtc.SetAlarmOne(alarm);
    rtc.LatchAlarmsTriggeredFlags();
}