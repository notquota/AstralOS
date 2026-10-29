
#pragma once

#include "icons.h"
#include "wallpaper.h"
#include "renderer.h"

#include "../taskbar/taskbar.h"
#include "../launcher/launcher.h"
#include "../windows/decorations.h"

namespace Astral::Desktop::AssetBindings {

struct Assets {
    const Image::Bitmap* wallpaper = nullptr;
    const Image::Bitmap* logo = nullptr;
    const Image::Bitmap* cursor = nullptr;

    const Image::Bitmap* files = nullptr;
    const Image::Bitmap* settings = nullptr;
    const Image::Bitmap* terminal = nullptr;
    const Image::Bitmap* monitor = nullptr;
    const Image::Bitmap* about = nullptr;
    const Image::Bitmap* calculator = nullptr;

    const Image::Bitmap* network = nullptr;
    const Image::Bitmap* volume = nullptr;
    const Image::Bitmap* battery = nullptr;

    const Image::Bitmap* close = nullptr;
    const Image::Bitmap* minimize = nullptr;
    const Image::Bitmap* maximize = nullptr;
    const Image::Bitmap* restore = nullptr;
};

inline void install(const Assets& assets) {
    Icons::initialize();

    if (assets.logo)
        Icons::register_icon("branding_astralos", assets.logo);

    if (assets.files)
        Icons::register_icon("apps_files", assets.files);

    if (assets.settings)
        Icons::register_icon("apps_settings", assets.settings);

    if (assets.terminal)
        Icons::register_icon("apps_terminal", assets.terminal);

    if (assets.monitor)
        Icons::register_icon("apps_system_monitor", assets.monitor);

    if (assets.about)
        Icons::register_icon("apps_about", assets.about);

    if (assets.calculator)
        Icons::register_icon("apps_calculator", assets.calculator);

    Wallpaper::set_bitmap(assets.wallpaper);
    Renderer::set_cursor(assets.cursor);

    Taskbar::IconSet taskbar{};
    taskbar.logo = assets.logo;
    taskbar.network = assets.network;
    taskbar.volume = assets.volume;
    taskbar.battery = assets.battery;

    taskbar.apps[0] = assets.files;
    taskbar.apps[1] = assets.settings;
    taskbar.apps[2] = assets.terminal;
    taskbar.apps[3] = assets.monitor;
    taskbar.apps[4] = assets.about;
    taskbar.apps[5] = assets.calculator;

    Taskbar::set_icons(taskbar);

    Launcher::set_icon(1, assets.files);
    Launcher::set_icon(2, assets.settings);
    Launcher::set_icon(3, assets.terminal);
    Launcher::set_icon(4, assets.monitor);
    Launcher::set_icon(5, assets.about);
    Launcher::set_icon(6, assets.calculator);

    Decorations::Icons controls{};
    controls.close = assets.close;
    controls.minimize = assets.minimize;
    controls.maximize = assets.maximize;
    controls.restore = assets.restore;

    Decorations::set_icons(controls);
}

}
