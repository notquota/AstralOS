
#pragma once

#include <stdint.h>
#include "../settings/registry.h"
#include "../windows/window.h"

namespace Astral::Desktop::Widgets::SettingControl {

constexpr int32_t HEIGHT = 56;

void render(
    const Settings::Item& item,
    const Windows::Rect& bounds,
    bool focused
);

// Returns true when the setting changes.
bool click(
    uint32_t settingId,
    const Windows::Rect& bounds,
    int32_t mouseX,
    int32_t mouseY
);

}
