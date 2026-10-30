#pragma once

#include <stdint.h>

namespace Astral::Mouse {

bool initialize();
bool initialized();

// Receives a byte from an existing PS/2 input route.
void feed(uint8_t data);

// Collects pending PS/2 mouse bytes when using
// controller polling instead of mouse IRQs.
void poll();

int32_t x();
int32_t y();

uint64_t revision();

// The button is no longer an instant teleportation
// between pressed and released. Technology!
bool left_down();

// Retained for older terminal code.
bool take_left_click();

int32_t take_scroll();
bool wheel_supported();

void set_ui_capture(bool captured);

}