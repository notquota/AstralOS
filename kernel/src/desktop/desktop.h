
#pragma once

#include <stdint.h>

namespace Astral::Desktop {

// Call this after the framebuffer has been initialized.
void set_display_size(uint32_t width, uint32_t height);

// Launch the graphical Desktop.
void run();

// Check whether the graphical Desktop is active.
bool running();

}
