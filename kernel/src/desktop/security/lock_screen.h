
#pragma once

namespace Astral::Desktop::LockScreen {

// Draws the lock screen when locked.
// Returns true when the lock screen is active.
bool render();

// Pass each key from Keyboard::read_char().
// Returns true if the key was consumed.
bool handle_key(char key);

}
