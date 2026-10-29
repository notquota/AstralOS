
#pragma once

#include <stdint.h>
#include "../launcher/app_registry.h"
#include "../windows/window.h"
#include "../graphics/image.h"

namespace Astral::Desktop::Taskbar {

constexpr int32_t HEIGHT = 58;

enum class ActionType : uint8_t {
    None,
    ToggleLauncher,
    LaunchApp,
    FocusWindow,
    RestoreWindow,
    MinimizeWindow,
    OpenQuickSettings
};

struct Action {
    ActionType type = ActionType::None;
    uint32_t target = 0;
};

struct IconSet {
    const Image::Bitmap* logo = nullptr;
    const Image::Bitmap* network = nullptr;
    const Image::Bitmap* volume = nullptr;
    const Image::Bitmap* battery = nullptr;

    // Null entries use a text fallback.
    const Image::Bitmap* apps[32]{};
};

void initialize();
void set_icons(const IconSet& icons);

void render(int32_t screenWidth, int32_t screenHeight);

Action click(
    int32_t x,
    int32_t y,
    int32_t screenWidth,
    int32_t screenHeight
);

}
