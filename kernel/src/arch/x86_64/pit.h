
#pragma once

#include <stdint.h>

namespace Astral::Timer {

// Waits approximately 50 milliseconds.
// Returns false if the timer fails to respond.
bool wait_50ms();

// Waits for a requested number of milliseconds,
// rounded up to 50 ms intervals.
bool wait_ms(uint32_t milliseconds);

}