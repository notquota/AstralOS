
#pragma once

#include <stdint.h>

namespace Astral::Mouse {

bool initialize();
bool initialized();

void feed(uint8_t data);

int32_t x();
int32_t y();

uint64_t revision();

bool take_left_click();

// Returns accumulated scroll-line movement.
// Positive = up, negative = down.
// Reading consumes the pending movement.
int32_t take_scroll();

bool wheel_supported();

void set_ui_capture(bool captured);

}
