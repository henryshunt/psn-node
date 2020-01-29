#include <Preferences.h>

#include "globals.h"


RTC_DATA_ATTR char mac_address[18] = { '\0' };

RTC_DATA_ATTR bool NETWORK_ENTERPRISE;
RTC_DATA_ATTR char NETWORK_NAME[32] = { '\0' };
RTC_DATA_ATTR char NETWORK_USERNAME[64] = { '\0' };
RTC_DATA_ATTR char NETWORK_PASSWORD[64] = { '\0' };
RTC_DATA_ATTR char LOGGER_ADDRESS[32] = { '\0' };
RTC_DATA_ATTR uint16_t LOGGER_PORT;
RTC_DATA_ATTR uint8_t NETWORK_TIMEOUT;
RTC_DATA_ATTR uint8_t LOGGER_TIMEOUT;


/*
    Loads the MAC address of the device into a global variable
 */
void load_mac_address()
{
    uint8_t mac_out[6];
    esp_efuse_mac_get_default(mac_out);

    sprintf(mac_address, "%x:%x:%x:%x:%x:%x", mac_out[0], mac_out[1], mac_out[2],
        mac_out[3], mac_out[4], mac_out[5]);
}

/*
    Loads configuration from NVS into global variables and checks value validity
 */
bool load_configuration(bool* valid_out)
{
    Preferences preferences;
    if (!preferences.begin("psn", true)) return false;
    
    NETWORK_ENTERPRISE = preferences.getBool("nent");
    strcpy(NETWORK_NAME, preferences.getString("nnam").c_str());
    strcpy(NETWORK_USERNAME, preferences.getString("nunm").c_str());
    strcpy(NETWORK_PASSWORD, preferences.getString("npwd").c_str());
    strcpy(LOGGER_ADDRESS, preferences.getString("ladr").c_str());
    LOGGER_PORT = preferences.getUShort("lprt");
    NETWORK_TIMEOUT = preferences.getUChar("tnet");
    LOGGER_TIMEOUT = preferences.getUChar("tlog");
    preferences.end();

    bool valid = true;

    // Check validity of loaded configuration
    if (strlen(NETWORK_NAME) == 0) valid = false;
    if (NETWORK_ENTERPRISE && (strlen(NETWORK_USERNAME) == 0 || strlen(NETWORK_PASSWORD) == 0))
        valid = false;

    if (strlen(LOGGER_ADDRESS) == 0) valid = false;
    if (LOGGER_PORT < 1024) valid = false;
    if (NETWORK_TIMEOUT < 1 || NETWORK_TIMEOUT > 13) valid = false;
    if (LOGGER_TIMEOUT < 1 || LOGGER_TIMEOUT > 13) valid = false;

    *valid_out = valid;
    return true;
}