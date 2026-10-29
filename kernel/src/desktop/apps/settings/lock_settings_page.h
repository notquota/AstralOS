
#pragma once

#include <stdint.h>
#include "../../windows/window.h"

namespace Astral::Desktop::Apps::LockSettingsPage {

enum class Section : uint8_t {
    Desktop,
    Main
};

enum class Action : uint8_t {
    None,
    Changed,
    LockDesktop,
    LockMain
};

void initialize();

void set_section(Section section);
Section section();

void render(const Windows::Rect& bounds);

Action click(
    const Windows::Rect& bounds,
    int32_t mouseX,
    int32_t mouseY
);

}
