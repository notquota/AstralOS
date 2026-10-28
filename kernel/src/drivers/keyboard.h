
#pragma once

namespace Astral::Keyboard {

// Initialize the PS/2 keyboard.
bool initialize();

// Check keyboard initialization status.
bool initialized();

// Read an available keyboard character.
// Returns '\0' when no character is available.
char read_char();

// Wait until a keyboard character is received.
// Mouse packets are also processed while waiting.
char wait_char();

}
