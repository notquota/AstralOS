#include "serial.h"
#include "../arch/x86_64/lowlevel.h"
#include <stdint.h>

namespace Astral::Serial {

static constexpr uint16_t COM1 = 0x3F8;
static bool ready = false;

void initialize() {
    ready = false;

    astral_out8(COM1 + 1, 0x00);
    astral_out8(COM1 + 3, 0x80);
    astral_out8(COM1 + 0, 0x03);
    astral_out8(COM1 + 1, 0x00);
    astral_out8(COM1 + 3, 0x03);
    astral_out8(COM1 + 2, 0xC7);
    astral_out8(COM1 + 4, 0x1E);

    astral_out8(COM1 + 0, 0xAE);

    if (astral_in8(COM1 + 0) != 0xAE)
        return;

    astral_out8(COM1 + 4, 0x0F);
    ready = true;
}

bool available() {
    return ready;
}

void put(char character) {
    if (!ready)
        return;

    uint32_t timeout = 100000;

    while (!(astral_in8(COM1 + 5) & 0x20)) {
        if (--timeout == 0)
            return;
    }

    astral_out8(COM1, character);
}

void write(const char* message) {
    if (!message)
        return;

    while (*message) {
        if (*message == '\n')
            put('\r');

        put(*message++);
    }
}

}