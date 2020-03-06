#include <Wire.h>
#include <RtcDS3231.h>


#ifndef HELPERS_H
#define HELPERS_H

#define SERIAL_TIMEOUT 5 // Number of seconds to wait for serial data at power on
#define ALLOWED_INTERVALS { 1, 2, 5, 10, 15, 20, 30 } // The allowed intervals
// between reports in minutes
#define ALLOWED_INTERVALS_LEN 7 // Number of elements in ALLOWED_INTERVALS
#define BUFFER_CAPACITY 208 // Maximum number of reports to store in the buffer
#define RTC_SQUARE_WAVE_PIN GPIO_NUM_35 // The pin connected to the RTC SQW pin
#define ALARM_SET_THRESHOLD 2 // Number of seconds of sleep to guarantee before an
// alarm fires (precaution to ensure device sleeps properly before alarm triggers)


// The possible results of transmissions to the logging server
enum RequestResult { Success, Fail, NoSession };

// Represents a session (tells the sensor node how to record and transmit reports)
struct session_t
{
    uint16_t session_id;
    uint8_t interval;
    uint8_t batch_size;
};

// Represents a collection of sensor values for a specific time (a report)
struct report_t
{
    uint32_t time;
    float airt;
    float relh;
    float batv;
};


int round_up_multiple(int, int);
void format_time(char*, const RtcDateTime&);
#endif