#include "main.h"
#include "utilities/utilities.h"
#include "utilities/buffer.h"
#include "serial.h"
#include "transmit.h"
#include <Preferences.h>
#include <nvs_flash.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <stdint.h>


char cfgNetworkName[32] = { '\0' };
bool cfgIsEnterprise;
char cfgNetworkUsername[64] = { '\0' };
char cfgNetworkPassword[64] = { '\0' };
char cfgServerAddress[32] = { '\0' };
uint16_t cfgServerPort;

char macAddress[18] = { '\0' };
RtcDS3231<TwoWire> ds3231(Wire);

/**
 * The mode the device is or should be in when it boots up or wakes from deep sleep.
 */
RTC_DATA_ATTR static int bootMode = 0;

/**
 * The number of attempts made to get the instructions for this sensor node.
 */
RTC_DATA_ATTR static int instructionsCheckCount = 0;

/**
 * The instructions for this sensor node.
 */
RTC_DATA_ATTR static instructions_t instructions;

/**
 * A circular buffer for storing observations for transmission.
 */
RTC_DATA_ATTR static buffer_t buffer;

/**
 * The array of observations maintained by the buffer.
 */
RTC_DATA_ATTR static observation_t observations[BUFFER_CAPACITY + 1];


static bool loadConfiguration(bool&);
static void setFirstObservationAlarm();
static void observe();
static void generateObservation(const RtcDateTime&);
static void serialiseObservation(const observation_t&, char* const);
static void setRtcAlarm(const RtcDateTime&);


/**
 * Performs initialisation, manages retrieval of the instructions (retrying on error) for
 * this sensor node, and calls the routine to generate and transmit an observation.
 */
void setup()
{
    uint8_t macTemp[6];
    esp_efuse_mac_get_default(macTemp);

    sprintf(macAddress, "%02x:%02x:%02x:%02x:%02x:%02x", macTemp[0],
        macTemp[1], macTemp[2], macTemp[3], macTemp[4], macTemp[5]);

    bool configValid;
    if (!loadConfiguration(configValid))
        esp_deep_sleep_start();

    if (bootMode == 0) // Booted from power off
    {
        // Permanently enter serial mode if any serial data is received
        trySerialMode();
    }

    ds3231.Begin();
    ds3231.SetSquareWavePin(DS3231SquareWavePin_ModeAlarmOne);

    if (!configValid || !ds3231.IsDateTimeValid())
        esp_deep_sleep_start();
    

    if (bootMode == 0) // Booted from power off
    {
        if (!networkConnect() || !serverConnect() || !serverSubscribe() ||
            !serverInstructions(instructions) || instructions.isNull)
        {
            bootMode = 1;
            setRtcAlarm(ds3231.GetDateTime() + 60);
            esp_sleep_enable_ext0_wakeup(RTC_SQUARE_WAVE_PIN, 0);
            esp_deep_sleep_start();
        }
        else
        {
            bootMode = 2;
            setFirstObservationAlarm();
        }
    }
    else if (bootMode == 1) // Woken from sleep but has no instructions
    {
        instructionsCheckCount++;

        if (!networkConnect() || !serverConnect() || !serverSubscribe() ||
            !serverInstructions(instructions) || instructions.isNull)
        {
            if (instructionsCheckCount < INSTRUCTIONS_CHECKS)
            {
                setRtcAlarm(ds3231.GetDateTime() + 60);
                esp_sleep_enable_ext0_wakeup(RTC_SQUARE_WAVE_PIN, 0);
            }

            esp_deep_sleep_start();
        }
        else
        {
            bootMode = 2;
            setFirstObservationAlarm();
        }
    }
    else observe(); // Woken from sleep and has instructions
}

void loop() { }

/**
 * Loads configuration data from the device's non-volatile storage into the global
 * variables prefixed with cfg, and checks the validity of the values.
 * @param validOut Will be set to indicate whether the loaded configuration data is valid.
 * @return An indication of success or failure of reading the configuration data.
 */
static bool loadConfiguration(bool& validOut)
{
    Preferences preferences;
    if (!preferences.begin("psn", false))
        return false;

    strcpy(cfgNetworkName, preferences.getString("nnam").c_str());
    cfgIsEnterprise = preferences.getBool("nent");
    strcpy(cfgNetworkUsername, preferences.getString("nunm").c_str());
    strcpy(cfgNetworkPassword, preferences.getString("npwd").c_str());
    strcpy(cfgServerAddress, preferences.getString("ladr").c_str());
    cfgServerPort = preferences.getUShort("lprt", 1883);
    preferences.end();

    bool isValid = true;

    // Check validity of loaded configuration
    if (strlen(cfgNetworkName) == 0 || (cfgIsEnterprise &&
        (strlen(cfgNetworkUsername) == 0 || strlen(cfgNetworkPassword)) == 0) ||
        strlen(cfgServerAddress) == 0 || cfgServerPort < 1024)
    {
        isValid = false;
    }

    validOut = isValid;
    return true;
}

/**
 * Sets an alarm to trigger the first observation, then goes to sleep.
 */
static void setFirstObservationAlarm()
{
    RtcDateTime time = ds3231.GetDateTime();
    time += 60 - time.Second(); // Move to start of next minute

    // Round up to next multiple of the interval (e.g. a 5 minute interval means
    // the next minute ending in a 0 or 5)
    time = roundUpMultiple(time, instructions.interval * 60);

    // Advance to next interval if currently too close to first available interval
    if (time - ds3231.GetDateTime() <= ALARM_THRESHOLD)
        time += instructions.interval * 60;

    setRtcAlarm(time);
    esp_sleep_enable_ext0_wakeup(RTC_SQUARE_WAVE_PIN, 0);
    esp_deep_sleep_start();
}

/**
 * Sets an alarm to trigger the next observation, generates an observation, transmits
 * observations stored in the buffer, then goes to sleep.
 */
static void observe()
{
    RtcDateTime now = ds3231.GetDateTime();
    RtcDateTime nextAlarm = now + (instructions.interval * 60);
    setRtcAlarm(nextAlarm);

    generateObservation(now);

    if (buffer.count() < instructions.batchSize ||
        !networkConnect() || !serverConnect() || !serverSubscribe())
    {
        esp_sleep_enable_ext0_wakeup(RTC_SQUARE_WAVE_PIN, 0);
        esp_deep_sleep_start();
    }

    // Transmit observations stored in the buffer as long as there's
    // enough time before the next observation alarm
    while (!buffer.isEmpty() && nextAlarm - ds3231.GetDateTime() >=
        NETWORK_TIMEOUT + ALARM_THRESHOLD)
    {
        observation_t* observation = buffer.peekRear(observations);

        char json[97] = { '\0' };
        serialiseObservation(*observation, json);

        instructions_t newInstructions;
        if (serverObservation(json, newInstructions))
        {
            buffer.popRear(observations);

            // If null then this sensor node has no more work to do
            if (!newInstructions.isNull)
                instructions = newInstructions;
            else esp_deep_sleep_start();
        }
        else break;
    }

    esp_sleep_enable_ext0_wakeup(RTC_SQUARE_WAVE_PIN, 0);
    esp_deep_sleep_start();
}

/**
 * Samples the sensors, produces an observation and pushes it to the observation buffer.
 * @param time The time of the observation.
 */
static void generateObservation(const RtcDateTime& time)
{
    observation_t observation =
        { (uint32_t)time, -99, -99, -99 };

    // Sample temperature and relative humidity
    Adafruit_BME680 bme680;
    if (bme680.begin(0x76))
    {
        bme680.setGasHeater(0, 0);
        bme680.setTemperatureOversampling(BME680_OS_8X);
        bme680.setHumidityOversampling(BME680_OS_2X);

        if (bme680.performReading())
        {
            observation.temperature = bme680.temperature;
            observation.relativeHumidity = bme680.humidity;
        }
    }

    // Sample battery voltage
    // observation.batteryVoltage = ...

    buffer.pushFront(observations, observation);
}

/**
 * Serialises an observation into a JSON string ready for transmission to the server.
 * @param obs The observation to serialise.
 * @param jsonOut The destination for the serialised observation JSON.
 */
static void serialiseObservation(const observation_t& obs, char* const jsonOut)
{
    int length = 0;
    length += sprintf(jsonOut, "{\"streamId\":%d", instructions.streamId);

    char time[20] = { '\0' };
    formatTime(RtcDateTime(obs.time), time);

    length += sprintf(jsonOut + length, ",\"time\":\"%s\"", time);

    if (obs.temperature != -99)
        length += sprintf(jsonOut + length, ",\"temp\":%.1f", obs.temperature);
    else length += sprintf(jsonOut + length, ",\"temp\":null");

    if (obs.relativeHumidity != -99)
        length += sprintf(jsonOut + length, ",\"relHum\":%.1f", obs.relativeHumidity);
    else length += sprintf(jsonOut + length, ",\"relHum\":null");

    if (obs.batteryVoltage != -99)
        length += sprintf(jsonOut + length, ",\"batVolt\":%.2f", obs.batteryVoltage);
    else length += sprintf(jsonOut + length, ",\"batVolt\":null");

    strcat(jsonOut + length, "}");
}

/**
 * Sets an alarm on the RTC.
 * @param time The time that the alarm should trigger at.
 */
static void setRtcAlarm(const RtcDateTime& time)
{
    DS3231AlarmOne alarm(time.Day(), time.Hour(), time.Minute(),
        time.Second(), DS3231AlarmOneControl_MinutesSecondsMatch);

    ds3231.SetAlarmOne(alarm);
    ds3231.LatchAlarmsTriggeredFlags();
}