
#include "keyboard.h"
#include "mouse.h"

#include "../arch/x86_64/lowlevel.h"
#include "../graphics/console.h"

#include <stdint.h>

namespace Astral::Keyboard {

static constexpr uint16_t DATA_PORT = 0x60;
static constexpr uint16_t STATUS_PORT = 0x64;

static bool ready = false;
static bool leftShift = false;
static bool rightShift = false;
static bool extended = false;
static bool pauseSequence = false;
static uint8_t pauseBytesLeft = 0;

static const char normalMap[128] = {
    0,
    27,
    '1','2','3','4','5','6','7','8','9','0','-','=',
    '\b',
    '\t',
    'q','w','e','r','t','y','u','i','o','p','[',']',
    '\n',
    0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,
    '\\',
    'z','x','c','v','b','n','m',',','.','/',
    0,
    '*',
    0,
    ' ',
};

static const char shiftMap[128] = {
    0,
    27,
    '!','@','#','$','%','^','&','*','(',')','_','+',
    '\b',
    '\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}',
    '\n',
    0,
    'A','S','D','F','G','H','J','K','L',':','"','~',
    0,
    '|',
    'Z','X','C','V','B','N','M','<','>','?',
    0,
    '*',
    0,
    ' ',
};

bool initialize() {
    ready = true;
    leftShift = false;
    rightShift = false;
    extended = false;
    pauseSequence = false;
    pauseBytesLeft = 0;

    return true;
}

bool initialized() {
    return ready;
}

static char decode(uint8_t scancode) {
    if (pauseSequence) {
        if (pauseBytesLeft)
            --pauseBytesLeft;

        if (pauseBytesLeft == 0)
            pauseSequence = false;

        return 0;
    }

    if (scancode == 0xE1) {
        pauseSequence = true;
        pauseBytesLeft = 5;
        extended = false;
        return 0;
    }

    if (scancode == 0xE0) {
        extended = true;
        return 0;
    }

    bool released = (scancode & 0x80) != 0;
    uint8_t code = scancode & 0x7F;

    if (extended) {
        extended = false;

        if (released)
            return 0;

        // Page Up
        if (code == 0x49) {
            Console::scroll_page_up();
            return 0;
        }

        // Page Down
        if (code == 0x51) {
            Console::scroll_page_down();
            return 0;
        }

        // End
        if (code == 0x4F) {
            Console::scroll_to_bottom();
            return 0;
        }

        // Extended keypad Enter
        if (code == 0x1C)
            return '\n';

        // Extended keypad /
        if (code == 0x35)
            return '/';

        return 0;
    }

    if (code == 0x2A) {
        leftShift = !released;
        return 0;
    }

    if (code == 0x36) {
        rightShift = !released;
        return 0;
    }

    if (released)
        return 0;

    if (code >= 128)
        return 0;

    return (leftShift || rightShift)
        ? shiftMap[code]
        : normalMap[code];
}

char read_char() {
    if (!ready)
        return 0;

    if (!(astral_in8(STATUS_PORT) & 0x01))
        return 0;

    // Read status first to identify the source.
    uint8_t status = astral_in8(STATUS_PORT);
    uint8_t data = astral_in8(DATA_PORT);

    // Status bit 5 marks second-port data on
    // compatible PS/2 controllers.
    if (status & 0x20) {
        Mouse::feed(data);
        return 0;
    }

    return decode(data);
}

char wait_char() {
    while (true) {
        char c = read_char();

        if (c)
            return c;

        astral_pause();
    }
}

}