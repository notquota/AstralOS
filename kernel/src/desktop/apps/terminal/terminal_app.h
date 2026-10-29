
#pragma once

#include <stdint.h>
#include "../../windows/window.h"

namespace Astral::Desktop::Apps::TerminalApp {

constexpr uint32_t MAX_LINES = 48;
constexpr uint32_t LINE_LENGTH = 96;
constexpr uint32_t INPUT_LENGTH = 128;

struct State {
    char lines[MAX_LINES][LINE_LENGTH];
    uint32_t lineCount;

    char input[INPUT_LENGTH];
    uint32_t inputLength;

    uint32_t scrollOffset;
    bool initialized;
};

void initialize();

void render(const Windows::Rect& bounds);

void handle_key(char key);
void scroll(int32_t amount);

void clear();

const State& state();

}
