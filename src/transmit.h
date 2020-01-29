#include <AsyncMqttClient.h>

#include "helpers/helpers.h"


bool network_connect();
bool is_network_connected();

bool logger_connect();
bool is_logger_connected();

bool logger_subscribe();
RequestResult logger_get_session(session_t*);
RequestResult logger_send_report(const char*);

void logger_on_subscribe(uint16_t, uint8_t);
void logger_on_message(char*, char*,
    AsyncMqttClientMessageProperties, size_t, size_t, size_t);