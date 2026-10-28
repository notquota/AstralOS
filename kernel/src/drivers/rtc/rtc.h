
#pragma once

#include <stdint.h>

namespace Astral::RTC {

struct DateTime {
    uint16_t year;
    uint8_t month;
    uint8_t day;

    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

// Read a calendar snapshot from the CMOS RTC.
// Does not configure periodic interrupts.
bool read(DateTime& result);

// Validate that a calendar reading is plausible.
bool valid(const DateTime& time);

}
