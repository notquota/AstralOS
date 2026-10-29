
#include "app_registry.h"

namespace Astral::Desktop::AppRegistry {

static Application applications[] = {
    {1, "Files", "apps_files",
        Category::System, true, true},

    {2, "Settings", "apps_settings",
        Category::System, true, true},

    {3, "Terminal", "apps_terminal",
        Category::Development, true, true},

    {4, "System Monitor", "apps_system_monitor",
        Category::System, true, true},

    {5, "About AstralOS", "apps_about",
        Category::System, true, false},

    {6, "Calculator", "apps_calculator",
        Category::Utilities, false, false}
};

static constexpr uint32_t COUNT =
    sizeof(applications) / sizeof(applications[0]);

void initialize() {
    for (uint32_t i = 0; i < COUNT; ++i) {
        applications[i].available = i < 5;
        applications[i].pinned = i < 4;
    }
}

uint32_t count() {
    return COUNT;
}

const Application* at(uint32_t index) {
    return index < COUNT
        ? &applications[index]
        : nullptr;
}

const Application* find(AppId id) {
    for (uint32_t i = 0; i < COUNT; ++i)
        if (applications[i].id == id)
            return &applications[i];

    return nullptr;
}

bool set_pinned(AppId id, bool pinned) {
    for (uint32_t i = 0; i < COUNT; ++i) {
        if (applications[i].id != id)
            continue;

        applications[i].pinned = pinned;
        return true;
    }

    return false;
}

bool set_available(AppId id, bool available) {
    for (uint32_t i = 0; i < COUNT; ++i) {
        if (applications[i].id != id)
            continue;

        applications[i].available = available;
        return true;
    }

    return false;
}

}
