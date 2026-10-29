
#pragma once
#include <stdint.h>

namespace Astral::Desktop::SystemStatus {

struct Battery {
    bool available;
    bool charging;
    uint8_t percent;
};

struct Clock {
    bool available;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

using BatteryReader = bool (*)(Battery&);

void initialize();
void set_battery_reader(BatteryReader reader);

void refresh();

Battery battery();
Clock clock();

// The returned strings are stored internally.
const char* time_text();
const char* battery_text();

}
