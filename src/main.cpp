/*
    Deals with device startup as well as the main logging routine responsible for
    generating and transmitting reports. Core program flow is here.
 */

#include <stdint.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

#include "main.h"
#include "helpers/globals.h"
#include "helpers/helpers.h"
#include "helpers/buffer.h"
#include "serial.h"
#include "transmit.h"


RTC_DATA_ATTR int boot_mode = 0;
RTC_DATA_ATTR int session_check_count = 0;
RTC_DATA_ATTR session_t session;
RTC_DATA_ATTR report_buffer_t buffer;
RTC_DATA_ATTR report_t reports[BUFFER_CAPACITY + 1];


/*
    Performs initialisation, manages the retrieval of the active session for this
    sensor node (repeats on error) and calls the reporting routine.
 */
void setup()
{
    rtc.Begin();
    if (boot_mode == 0) // Booted from power off
    {
        uint8_t mac_temp[6];
        esp_efuse_mac_get_default(mac_temp);

        sprintf(mac_address, "%x:%x:%x:%x:%x:%x", mac_temp[0], mac_temp[1],
            mac_temp[2], mac_temp[3], mac_temp[4], mac_temp[5]);

        bool config_valid;
        if (!load_configuration(&config_valid)) esp_deep_sleep_start();

        // Permanently enter serial mode if received serial data
        try_serial_mode();

        if (!config_valid) esp_deep_sleep_start();
        if (!is_rtc_time_valid()) esp_deep_sleep_start();
        rtc.SetSquareWavePin(DS3231SquareWavePin_ModeAlarmOne);

        RtcDateTime alarm_time = rtc.GetDateTime() + 60;
        set_rtc_alarm(alarm_time);

        if (!connect_and_get_session())
        {
            boot_mode = 1;
            esp_sleep_enable_ext0_wakeup(RTC_SQUARE_WAVE_PIN, 0);
            esp_deep_sleep_start();
        } else set_first_alarm();
    }
    else if (boot_mode == 1) // Woken from sleep but has no session
    {
        if (!is_rtc_time_valid()) esp_deep_sleep_start();

        session_check_count++;
        RtcDateTime alarm_time = rtc.GetDateTime() + 60;
        set_rtc_alarm(alarm_time);

        if (!connect_and_get_session())
        {
            if (session_check_count < SESSION_CHECKS)
                esp_sleep_enable_ext0_wakeup(RTC_SQUARE_WAVE_PIN, 0);
            esp_deep_sleep_start();
        } else set_first_alarm();
    }
    else reporting_routine(); // Woken from sleep and must report
}

/*
    Waits a certain amount of time for data to be received on the serial port
    and if it is, goes into an infinite loop to respond to serial commands.
 */
void try_serial_mode()
{
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
}

/*
    Attempts to connect to the WiFi network and logging server, then attempts to
    get the active sesion for this sensor node. Returns a boolean indicating
    success or failure, and fails if no session was gotten.
 */
bool connect_and_get_session()
{
    if (!network_connect() || !logger_connect()) return false;
    if (!logger_subscribe()) return false;

    RequestResult session_status = logger_get_session(&session);
    if (session_status == RequestResult::Fail ||
        session_status == RequestResult::NoSession)
    { return false; }

    return true;
}

/*
    Switches to boot mode 2, sets an alarm to trigger the first report, then goes
    to sleep.
 */
void set_first_alarm()
{
    boot_mode = 2;

    RtcDateTime first_alarm = rtc.GetDateTime();
    first_alarm += (60 - first_alarm.Second()); // Move to start of next minute
    first_alarm = round_up_multiple(first_alarm, session.interval * 60); // Round
    // up to next multiple of the interval (e.g. a 5 minute interval rounds to
    // the next minute ending in a 0 or 5)

    // Advance to next interval if currently too close to first available interval
    if (first_alarm - rtc.GetDateTime() <= ALARM_SET_THRESHOLD)
        first_alarm += session.interval * 60;

    set_rtc_alarm(first_alarm);
    esp_sleep_enable_ext0_wakeup(RTC_SQUARE_WAVE_PIN, 0);
    esp_deep_sleep_start();
}

void loop() { }


/*
    Sets alarm to trigger the next report, generates a report, transmits all
    reports in the buffer, then goes to sleep.
 */
void reporting_routine()
{
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
        bme680.setGasHeater(0, 0);
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