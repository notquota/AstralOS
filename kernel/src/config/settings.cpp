
#include "settings.h"

namespace Astral::Config {

static constexpr Settings DEFAULTS = {
    Accent::Violet,
    true,
    1,
    3,
    true,
    Language::English,
    true,
    0
};

static Settings current = DEFAULTS;
static uint64_t settingsRevision = 0;

void initialize() {
    current = DEFAULTS;
    ++settingsRevision;
}

void reset_defaults() {
    current = DEFAULTS;
    ++settingsRevision;
}

const Settings& get() {
    return current;
}

void set(const Settings& settings) {
    current = settings;

    switch (current.accent) {
        case Accent::Violet:
        case Accent::Blue:
        case Accent::Green:
        case Accent::Amber:
            break;
        default:
            current.accent = Accent::Violet;
            break;
    }

    if (current.pointerSpeed < 1)
        current.pointerSpeed = 1;
    if (current.pointerSpeed > 3)
        current.pointerSpeed = 3;

    if (current.scrollLines < 1)
        current.scrollLines = 1;
    if (current.scrollLines > 9)
        current.scrollLines = 9;

    if (current.utcOffsetMinutes < -720)
        current.utcOffsetMinutes = -720;
    if (current.utcOffsetMinutes > 840)
        current.utcOffsetMinutes = 840;

    // English is the only installed language.
    current.language = Language::English;

    ++settingsRevision;
}

uint64_t revision() {
    return settingsRevision;
}

uint32_t accent_color() {
    switch (current.accent) {
        case Accent::Blue: return 0x60A5FA;
        case Accent::Green: return 0x4ADE80;
        case Accent::Amber: return 0xFBBF24;
        default: return 0xA78BFA;
    }
}

const char* language_name() {
    return "English";
}

}
