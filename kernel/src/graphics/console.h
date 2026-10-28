
#pragma once

#include <stdint.h>

namespace Astral::Console {

void initialize();

void write(
    const char* message,
    uint32_t color
);

void newline();
void backspace();

// Positive lines scroll toward newer output.
// Negative lines scroll toward older output.
void scroll_lines(int32_t lines);

void scroll_page_up();
void scroll_page_down();
void scroll_to_bottom();

bool is_scrolled();

}
