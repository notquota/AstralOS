
#pragma once

#include <stdint.h>
#include "../../windows/window.h"
#include "../../settings/registry.h"

namespace Astral::Desktop::Apps::SettingsApp {

using Category = Settings::Category;

enum class Action : uint8_t {
    None,
    Redraw,
    LockDesktop,
    LockMain
};

struct State {
    Category category;
    uint32_t sidebarOffset;
    uint32_t settingOffset;
    bool initialized;
};

void initialize();
void select(Category category);

void render(const Windows::Rect& bounds);

Action click(
    const Windows::Rect& bounds,
    int32_t mouseX,
    int32_t mouseY
);

void scroll(int32_t steps);

const State& state();

}
