#ifndef UTILITIES_H
#define UTILITIES_H

#include <RtcDS3231.h>


/**
 * The number of seconds to time out after when performing network-related actions.
 */
#define NETWORK_TIMEOUT 10

/**
 * The number of seconds to wait for serial data when the device boots for the first time.
 */
#define SERIAL_TIMEOUT 5

/**
 * The maximum length of a serial command.
 */
#define SERIAL_COMMAND_LENGTH 200

/**
 * The number of times to repeat attempting to get the instructions for this sensing node.
 */
#define INSTRUCTIONS_CHECKS 15

/**
 * An array literal containing the allowed observing intervals in minutes.
 */
#define ALLOWED_INTERVALS { 1, 2, 5, 10, 15, 20, 30 }

/**
 * The number of elements in ALLOWED_INTERVA.
 */
#define ALLOWED_INTERVALS_LEN 7

/**
 * The maximum number of observations to store in the buffer.
 */
#define BUFFER_CAPACITY 205

/**
 * The GPIO pin connected to the DS3231's INT/SQW pin.
 */
#define RTC_SQUARE_WAVE_PIN GPIO_NUM_35

/**
 * The number of seconds of sleep to guarantee before an alarm triggers (this is a
 * precaution to ensure the device properly enters sleep mode before the alarm triggers).
 */
#define ALARM_THRESHOLD 1


/**
 * Represents the device's configuration data.
 */
struct config_t
{
    /**
     * The name of the WiFi network to connect to.
     */
    char networkName[32] = { '\0' };

    /**
     * Is the WiFi network an enterprise network? An enterprise network requires credentials
     * with a username and password.
     */
    bool isEnterprise;

    /**
     * The username to connect to the WiFi network with, if it is an enterprise network.
     */
    char networkUsername[64] = { '\0' };

    /**
     * The password to connect to the WiFi network with.
     */
    char networkPassword[64] = { '\0' };

    /**
     * The address of the MQTT server to connect to.
     */
    char serverAddress[32] = { '\0' };

    /**
     * The port to connect to the MQTT server through.
     */
    uint16_t serverPort;
};

/**
 * Represents information instructing a sensing node what to do.
 */
struct instructions_t
{
    /**
     * Used to indicate that the struct is in a null state, with no valid data.
     */
    bool isNull = true;

    /**
     * The ID of the stream that the sensing node is making observations for.
     */
    uint16_t streamId;

    /**
     * The interval between observations, in minutes.
     */
    uint8_t interval;

    /**
     * The number of observations to make before transmitting them all at once.
     */
    uint8_t batchSize;
};

/**
 * Represents sensor measurements at a specific time.
 */
struct observation_t
{
    /**
     * The time of the observation. Stored as the number of seconds since January 1st
     * 2000.
     */
    uint32_t time;

    float temperature;
    float relativeHumidity;
    float batteryVoltage;
};


/**
 * Represents the available server-related asynchronous actions.
 */
enum ServerAction
{
    /**
     * No action.
     */
    None,

    /**
     * Subscribing to the MQTT server.
     */
    Subscribe,

    /**
     * Requesting instructions for the sensing node.
     */
    Instructions,

    /**
     * Transmitting an observation.
     */
    Observation
};


/**
 * Rounds a number up to a multiple of another number.
 * @param number The number to round.
 * @param multiple The number will be rounded up to a multiple of this number.
 * @returns The rounded number.
 */
int roundUpMultiple(int number, int multiple);

/**
 * Formats a time into a string of the format yyyy-MM-ddTHH:mm:ss.
 * @param time The time to serialise.
 * @param timeOut The destination for the serialised time.
*/
void formatTime(const RtcDateTime& time, char* const timeOut);

#endif