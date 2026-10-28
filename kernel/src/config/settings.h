
#pragma once

#include <stdint.h>

namespace Astral::Config {

enum class Accent : uint8_t {
    Violet,
    Blue,
    Green,
    Amber
};

enum class Language : uint8_t {
    English
};

struct Settings {
    Accent accent;
    bool darkMode;

    uint32_t pointerSpeed;
    uint32_t scrollLines;
    bool showPointer;

    Language language;
    bool use24HourClock;
    int32_t utcOffsetMinutes;
};

void initialize();
void reset_defaults();

const Settings& get();
void set(const Settings& settings);

uint64_t revision();

uint32_t accent_color();
const char* language_name();

}
