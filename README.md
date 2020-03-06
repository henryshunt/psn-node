# Phenotyping Sensor Network: Node
PSN is a network of wireless battery-powered sensing nodes designed specifically for the phenotyping domain. This codebase contains the code that runs on the individual sensor nodes that make up the network. Target hardware is an ESP32 PICO Kit V4 with a BME680 and DS3231.

# Usage
- Open the project in PlatformIO, compile and upload to the board.
- Use the [PSN Node Administrator](https://github.com/henryshunt/psn-node-admin) to configure the device

# Dependencies
- RTC (Michael Miller)
- AsyncMQTTClient
- Adafruit Unified Sensor
- Adafruit BME680
- ArduinoJson
