#include <RtcDS3231.h>

#include "helpers/helpers.h"


void setup();
void loop();

void wake_routine();
void generate_report(const RtcDateTime&);
void serialise_report(char*, const report_t&, const session_t&);