#include "AsyncMqttClient.h"


bool network_connect();

bool logger_connect();
bool logger_subscribe();
bool logger_session();
bool logger_report(const char* report, uint32_t time);

void logger_on_subscribe(uint16_t packet_id, uint8_t qos);
void logger_on_message(char* topic, char* payload,
    AsyncMqttClientMessageProperties properties, size_t length, size_t index,
    size_t total);