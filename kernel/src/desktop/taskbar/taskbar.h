
#pragma once

#include <stdint.h>

#include "../graphics/image.h"
#include "../windows/window.h"
#include "../launcher/app_registry.h"

namespace Astral::Desktop::Taskbar {

// ============================================
// ASTRALOS TASKBAR
// ============================================
//
// Congratulations.
// This is now an actual taskbar header.
//
// Previously, this file was impersonating
// a clock and committing identity fraud.
//
// The compiler has pressed charges.

constexpr int32_t HEIGHT = 60;

constexpr uint32_t MAX_APP_ICONS = 32;

// The PNG employment agency.
//
// Every icon gets assigned a job.
// Except the ones that don't exist.
// Those remain professionally unemployed.
struct IconSet {
    const Image::Bitmap* logo = nullptr;

    const Image::Bitmap* network = nullptr;
    const Image::Bitmap* volume = nullptr;
    const Image::Bitmap* battery = nullptr;

    const Image::Bitmap* apps[MAX_APP_ICONS]{};
};

// What the taskbar wants to do.
//
// It doesn't actually do these things.
// It files paperwork with DesktopManager,
// who has to deal with the consequences.
enum class ActionType : uint8_t {
    None,

    ToggleLauncher,
    LaunchApp,

    FocusWindow,
    RestoreWindow,
    MinimizeWindow,

    OpenQuickSettings
};

// A single request for the Desktop Manager.
//
// One action.
// One target.
// Approximately fourteen opportunities
// for a completely unrelated kernel panic.
struct Action {
    ActionType type = ActionType::None;
    uint32_t target = 0;
};

// The taskbar wakes up.
// Nobody is happy about this.
void initialize();

// Assign actual icons instead of making
// the taskbar draw mysterious rectangles
// and calling them applications.
void set_icons(const IconSet& icons);

// Draw the taskbar at the bottom of the screen.
//
// Yes, the bottom.
// The TOP status bar is somebody else's problem.
//
// We learned this the hard way.
void render(
    int32_t screenWidth,
    int32_t screenHeight
);

// Figure out what the user clicked.
//
// Hopefully something.
// Previously, clicking this interface
// had roughly the same effect as
// yelling at a disconnected printer.
Action click(
    int32_t x,
    int32_t y,
    int32_t screenWidth,
    int32_t screenHeight
);

} // namespace Astral::Desktop::Taskbar
