/*
    Holds various variables used accross the codebase, most importantly the
    device configuration.
 */

#include <Wire.h>
#include <nvs_flash.h>
#include <Preferences.h>
#include <RtcDS3231.h>

#include "globals.h"


RTC_DATA_ATTR char mac_address[18] = { '\0' };

RTC_DATA_ATTR char network_name[32] = { '\0' };
RTC_DATA_ATTR bool is_enterprise_network;
RTC_DATA_ATTR char network_username[64] = { '\0' };
RTC_DATA_ATTR char network_password[64] = { '\0' };
RTC_DATA_ATTR char logger_address[32] = { '\0' };
RTC_DATA_ATTR uint16_t logger_port;
RTC_DATA_ATTR uint8_t network_timeout;
RTC_DATA_ATTR uint8_t logger_timeout;

RtcDS3231<TwoWire> rtc(Wire);


/*
    Loads the device configuration from non-volatile storage into the global
    variables and checks the validity of the values. Returns a boolean indicating
    the success or failure of reading the NVS.

    - valid_out: a boolean that will be set to indicate whether the loaded
    configuration is valid or not
 */
bool load_configuration(bool* valid_out)
{
    Preferences preferences;
    if (!preferences.begin("psn", false)) return false;

    strcpy(network_name, preferences.getString("nnam").c_str());
    is_enterprise_network = preferences.getBool("nent");
    strcpy(network_username, preferences.getString("nunm").c_str());
    strcpy(network_password, preferences.getString("npwd").c_str());
    strcpy(logger_address, preferences.getString("ladr").c_str());
    logger_port = preferences.getUShort("lprt", 1883);
    network_timeout = preferences.getUChar("tnet", 6);
    logger_timeout = preferences.getUChar("tlog", 6);
    preferences.end();

    bool valid = true;

    // Check validity of loaded configuration
    if (strlen(network_name) == 0) valid = false;
    if (is_enterprise_network && (strlen(network_username) == 0 ||
        strlen(network_password) == 0)) valid = false;
    if (strlen(logger_address) == 0) valid = false;
    if (logger_port < 1024) valid = false;
    if (network_timeout < 1 || network_timeout > 13) valid = false;
    if (logger_timeout < 1 || logger_timeout > 13) valid = false;

    *valid_out = valid;
    return true;
}