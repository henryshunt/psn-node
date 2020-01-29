#include "RtcDS3231.h"


#define SERIAL_TIMEOUT 5 // Number of seconds to wait for serial data at power on
#define BUFFER_SIZE 10 // Maximum number of reports to store in the buffer
#define RTC_SQUARE_WAVE_PIN GPIO_NUM_35 // What GPIO pin is the RTC SQW pin connected to?
#define ALARM_SET_THRESHOLD 2 // Number of seconds of sleep to guarantee before an alarm
// fires (precaution to ensure the device sleeps properly before the alarm triggers)


void setup();
void loop();

void wake_routine();
void generate_report(const RtcDateTime&);