
#pragma once

#include <stdint.h>
#include "../../windows/window.h"

namespace Astral::Desktop::Apps::FileManager {

constexpr uint32_t MAX_ENTRIES = 64;
constexpr uint32_t MAX_PATH = 256;
constexpr uint32_t MAX_NAME = 64;

enum class EntryType : uint8_t {
    File,
    Directory
};

struct Entry {
    char name[MAX_NAME];
    EntryType type;
    uint64_t size;
};

struct State {
    char currentPath[MAX_PATH];
    Entry entries[MAX_ENTRIES];
    uint32_t entryCount;
    uint32_t selectedIndex;
    uint32_t scrollOffset;
    bool initialized;
    bool sourceAvailable;
};

struct Provider {
    bool (*list)(
        const char* path,
        Entry* entries,
        uint32_t capacity,
        uint32_t& count
    );

    bool (*is_directory)(const char* path);
};

void initialize();

void set_provider(const Provider& provider);

bool navigate(const char* path);
bool up();
bool refresh();

void render(const Windows::Rect& bounds);

bool click(
    const Windows::Rect& bounds,
    int32_t mouseX,
    int32_t mouseY
);

void scroll(int32_t steps);

const State& state();

}
