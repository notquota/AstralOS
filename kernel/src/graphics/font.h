
#pragma once

#include <stdint.h>

namespace Astral::Graphics {

constexpr uint32_t FONT_WIDTH = 5;
constexpr uint32_t FONT_HEIGHT = 7;

// Each row uses its lowest five bits.
// Bit 4 represents the leftmost pixel.
const uint8_t* glyph(char character);

}
