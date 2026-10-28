
#include "pit.h"
#include "lowlevel.h"

namespace Astral::Timer {

static constexpr uint16_t PIT_COMMAND = 0x43;
static constexpr uint16_t PIT_CHANNEL2 = 0x42;
static constexpr uint16_t SPEAKER_PORT = 0x61;

// 50 ms at approximately 1.193182 MHz.
static constexpr uint16_t COUNT_50MS = 59659;

// This is a safety guard, not a time measurement.
static constexpr uint32_t POLL_LIMIT = 100000000;

bool wait_50ms() {
    uint8_t original = astral_in8(SPEAKER_PORT);

    // Disable speaker output and close timer gate.
    uint8_t control = original & 0xFC;
    astral_out8(SPEAKER_PORT, control);

    // Channel 2, low/high byte, mode 0, binary.
    astral_out8(PIT_COMMAND, 0xB0);

    astral_out8(
        PIT_CHANNEL2,
        static_cast<uint8_t>(COUNT_50MS & 0xFF)
    );

    astral_out8(
        PIT_CHANNEL2,
        static_cast<uint8_t>(COUNT_50MS >> 8)
    );

    // Start the countdown without speaker audio.
    astral_out8(SPEAKER_PORT, control | 0x01);

    bool completed = false;

    // In mode 0 the output remains low until
    // the programmed count reaches zero.
    for (uint32_t i = 0; i < POLL_LIMIT; ++i) {
        if (astral_in8(SPEAKER_PORT) & 0x20) {
            completed = true;
            break;
        }

        astral_pause();
    }

    // Stop the channel and restore speaker controls.
    astral_out8(
        SPEAKER_PORT,
        original & static_cast<uint8_t>(~0x03)
    );

    return completed;
}

bool wait_ms(uint32_t milliseconds) {
    uint32_t intervals =
        milliseconds / 50;

    if (milliseconds % 50 != 0)
        ++intervals;

    for (uint32_t i = 0; i < intervals; ++i) {
        if (!wait_50ms())
            return false;
    }

    return true;
}

}
