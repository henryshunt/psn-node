/**
 * Contains functions for connecting to the WiFi network, connecting to the server and
 * communicating with the server.
 */

#include "helpers/helpers.h"

/**
 * Connects to the WiFi network or times out. Note: I cannot guarantee that this function
 * will work properly when called multiple times. The only way to ensure the system is not
 * left in an unrecoverable state is to perform a reset before calling it again.
 * @return An indication of success or failure.
 */
bool networkConnect();

/**
 * Connects to the MQTT server or times out. Note: I cannot guarantee that this function
 * will work properly when called multiple times. The only way to ensure the system is not
 * left in an unrecoverable state is to perform a reset before calling it again.
 * @return An indication of success or failure.
 */
bool serverConnect();

/**
 * Subscribes to the inbound topic for this sensor node on the MQTT server, or times out.
 * @return An indication of success or failure.
 */
bool serverSubscribe();

/**
 * Requests the instructions for this sensor node or times out.
 * @param instrucOut The destination for the received instructions. If there are no
 * instructions available then the isNull attribute is set to true.
 * @return An indication of success or failure.
*/
bool serverInstructions(instructions_t& instrucOut);

/**
 * Transmits an observation and receives the instructions for this sensor node, or times
 * out.
 * @param obsJson A JSON string representing the observation to transmit.
 * @param instrucOut The destination for the received instructions. If there are no
 * instructions available then the isNull attribute is set to true.
 * @return An indication of success or failure.
*/
bool serverObservation(const char* const obsJson, instructions_t& instrucOut);