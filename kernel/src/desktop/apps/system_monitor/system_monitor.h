
#pragma once

#include <stdint.h>
#include "../../windows/window.h"

namespace Astral::Desktop::Apps::SystemMonitor {

enum class Page : uint8_t {
    Overview,
    CPU,
    Memory,
    Processes,
    Hardware
};

struct Snapshot {
    bool cpuAvailable;
    uint32_t cpuUsagePercent;
    uint32_t cpuCount;

    bool memoryAvailable;
    uint64_t totalMemoryBytes;
    uint64_t usedMemoryBytes;

    bool uptimeAvailable;
    uint64_t uptimeSeconds;

    bool processCountAvailable;
    uint32_t processCount;
};

using SnapshotReader = bool (*)(Snapshot&);

void initialize();
void set_provider(SnapshotReader reader);

void refresh();
void select(Page page);

void render(const Windows::Rect& bounds);

bool click(
    const Windows::Rect& bounds,
    int32_t mouseX,
    int32_t mouseY
);

const Snapshot& snapshot();
Page page();
bool data_available();

}
