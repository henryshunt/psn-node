#ifndef MAIN_H
#define MAIN_H

#include "utilities/utilities.h"
#include <RtcDS3231.h>
#include <Wire.h>

/**
 * The device's MAC address.
 */
extern char macAddress[18];

/**
 * The device's configuration data.
 */
extern config_t config;

/**
 * The device's real-time clock.
 */
extern RtcDS3231<TwoWire> ds3231;

/**
 * Loads configuration data from the device's non-volatile storage into the global
 * variables prefixed with cfg, and checks the validity of the values.
 * @param configOut The destination for the loaded configuration data.
 * @param validOut Will be set to indicate whether the loaded configuration data is valid.
 * @return An indication of success or failure of reading the configuration data.
 */
bool loadConfiguration(config_t& configOut, bool& validOut);

#endif