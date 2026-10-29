
#pragma once
#include <stdint.h>

namespace Astral::Desktop::AppRegistry {

using AppId = uint32_t;

enum class Category : uint8_t {
    System,
    Productivity,
    Utilities,
    Development
};

struct Application {
    AppId id;
    const char* name;
    const char* icon;
    Category category;
    bool available;
    bool pinned;
};

void initialize();

uint32_t count();
const Application* at(uint32_t index);
const Application* find(AppId id);

bool set_pinned(AppId id, bool pinned);
bool set_available(AppId id, bool available);

}
