#ifndef MAIN_H
#define MAIN_H

#include <RtcDS3231.h>
#include <Wire.h>

/**
 * The name of the WiFi network to connect to.
 */
extern char cfgNetworkName[32];

/**
 * Is the WiFi network an enterprise network? An enterprise network requires credentials
 * with a username and password.
 */
extern bool cfgIsEnterprise;

/**
 * The username to connect to the WiFi network with, if it is an enterprise network.
 */
extern char cfgNetworkUsername[64];

/**
 * The password to connect to the WiFi network with.
 */
extern char cfgNetworkPassword[64];

/**
 * The address of the MQTT server to connect to.
 */
extern char cfgServerAddress[32];

/**
 * The port to connect to the MQTT server through.
 */
extern uint16_t cfgServerPort;

/**
 * The device's MAC address.
 */
extern char macAddress[18];

/**
 * The device's real-time clock.
 */
extern RtcDS3231<TwoWire> ds3231;

#endif