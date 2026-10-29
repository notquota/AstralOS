
#pragma once

#include <stdint.h>
#include "../../windows/window.h"

namespace Astral::Desktop::Apps::BrowserApp {

enum class PageState : uint8_t {
    Home,
    Loading,
    Loaded,
    Offline,
    PluginMissing,
    Error
};

// The plugin owns rendering and networking.
// We just give it a place to live.
struct Plugin {
    bool (*online)() = nullptr;
    bool (*navigate)(const char* url) = nullptr;
    void (*reload)() = nullptr;
    void (*stop)() = nullptr;
    void (*render)(const Windows::Rect& area) = nullptr;
    void (*poll)() = nullptr;
};

void initialize();

void set_plugin(const Plugin& plugin);
bool plugin_available();

void render(const Windows::Rect& area);
bool click(const Windows::Rect& area, int32_t x, int32_t y);
bool handle_key(char key);

bool navigate(const char* url);
bool back();
bool forward();
void reload();

void poll();

const char* current_url();
PageState page_state();

}
