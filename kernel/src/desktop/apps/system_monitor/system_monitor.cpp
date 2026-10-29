
#include "system_monitor.h"
#include "../../graphics/image.h"

namespace Astral::Desktop::Apps::SystemMonitor {

namespace {

Snapshot current{};
SnapshotReader reader = nullptr;
Page activePage = Page::Overview;
bool available = false;

constexpr uint32_t BG = 0x142039;
constexpr uint32_t PANEL = 0x20314F;
constexpr uint32_t ACTIVE = 0x3E4E78;
constexpr uint32_t TEXT = 0xEDF2FF;
constexpr uint32_t MUTED = 0xA9B8D0;
constexpr uint32_t PURPLE = 0xA48AF8;

constexpr const char* PAGE_NAMES[] = {
    "Overview",
    "CPU",
    "Memory",
    "Processes",
    "Hardware"
};

bool contains(
    int32_t px, int32_t py,
    int32_t x, int32_t y,
    int32_t width, int32_t height
) {
    return px >= x && py >= y &&
           static_cast<int64_t>(px) <
               static_cast<int64_t>(x) + width &&
           static_cast<int64_t>(py) <
               static_cast<int64_t>(y) + height;
}

void format_number(
    uint64_t number,
    char* output,
    uint32_t capacity
) {
    if (!output || capacity == 0)
        return;

    char reverse[32];
    uint32_t n = 0;

    do {
        reverse[n++] =
            static_cast<char>('0' + number % 10);
        number /= 10;
    } while (number && n < sizeof(reverse));

    uint32_t index = 0;

    while (n && index + 1 < capacity)
        output[index++] = reverse[--n];

    output[index] = '\0';
}

void metric(
    int32_t x,
    int32_t y,
    int32_t width,
    const char* title,
    const char* value
) {
    Image::rectangle(
        x, y, width, 72,
        PANEL
    );

    Image::text(
        x + 13, y + 14,
        title, MUTED
    );

    Image::text(
        x + 13, y + 38,
        value, TEXT, 2
    );
}

void percentage_bar(
    int32_t x,
    int32_t y,
    int32_t width,
    uint32_t percentage
) {
    if (percentage > 100)
        percentage = 100;

    Image::rectangle(
        x, y, width, 12,
        0x2B3D5A
    );

    Image::rectangle(
        x, y,
        static_cast<int32_t>(
            static_cast<int64_t>(width) *
            percentage / 100
        ),
        12,
        PURPLE
    );
}

void unavailable(
    const Windows::Rect& bounds,
    const char* message
) {
    Image::text(
        bounds.x + 23,
        bounds.y + 115,
        message, MUTED
    );
}

void render_cpu(const Windows::Rect& r) {
    if (!current.cpuAvailable) {
        unavailable(r, "CPU telemetry unavailable");
        return;
    }

    char usage[24]{};
    char cores[24]{};

    format_number(
        current.cpuUsagePercent, usage, sizeof(usage)
    );
    format_number(
        current.cpuCount, cores, sizeof(cores)
    );

    metric(
        r.x + 20, r.y + 100,
        160, "CPU usage", usage
    );

    metric(
        r.x + 195, r.y + 100,
        160, "Logical CPUs", cores
    );

    percentage_bar(
        r.x + 20, r.y + 194,
        r.width - 40,
        current.cpuUsagePercent
    );
}

void render_memory(const Windows::Rect& r) {
    if (!current.memoryAvailable ||
        current.totalMemoryBytes == 0) {
        unavailable(r, "Memory telemetry unavailable");
        return;
    }

    const uint64_t mib = 1024ULL * 1024ULL;

    const uint64_t totalMiB =
        current.totalMemoryBytes / mib;

    const uint64_t usedMiB =
        current.usedMemoryBytes / mib;

    uint32_t percentage = 0;

    if (current.usedMemoryBytes >=
        current.totalMemoryBytes) {
        percentage = 100;
    } else {
        percentage = static_cast<uint32_t>(
            current.usedMemoryBytes * 100ULL /
            current.totalMemoryBytes
        );
    }

    char total[24]{};
    char used[24]{};

    format_number(totalMiB, total, sizeof(total));
    format_number(usedMiB, used, sizeof(used));

    metric(
        r.x + 20, r.y + 100,
        160, "Total MiB", total
    );

    metric(
        r.x + 195, r.y + 100,
        160, "Used MiB", used
    );

    percentage_bar(
        r.x + 20, r.y + 194,
        r.width - 40, percentage
    );
}

void render_processes(const Windows::Rect& r) {
    if (!current.processCountAvailable) {
        unavailable(r, "Process accounting unavailable");
        return;
    }

    char count[24]{};
    format_number(
        current.processCount, count, sizeof(count)
    );

    metric(
        r.x + 20, r.y + 100,
        220, "Processes", count
    );

    Image::text(
        r.x + 20, r.y + 195,
        "Detailed process list not provided",
        MUTED
    );
}

void render_hardware(const Windows::Rect& r) {
    Image::text(
        r.x + 22, r.y + 106,
        "Architecture: x86_64",
        TEXT
    );

    Image::text(
        r.x + 22, r.y + 141,
        "Hardware inventory provider pending",
        MUTED
    );
}

}

void initialize() {
    current = {};
    reader = nullptr;
    activePage = Page::Overview;
    available = false;
}

void set_provider(SnapshotReader provider) {
    reader = provider;
    refresh();
}

void refresh() {
    current = {};
    available = false;

    if (!reader)
        return;

    Snapshot result{};

    if (!reader(result))
        return;

    if (result.cpuUsagePercent > 100)
        result.cpuAvailable = false;

    if (result.memoryAvailable &&
        result.usedMemoryBytes > result.totalMemoryBytes)
        result.memoryAvailable = false;

    current = result;
    available = true;
}

void select(Page selected) {
    if (static_cast<uint32_t>(selected) <=
        static_cast<uint32_t>(Page::Hardware))
        activePage = selected;
}

void render(const Windows::Rect& r) {
    if (r.width <= 0 || r.height <= 0)
        return;

    Image::rectangle(
        r.x, r.y, r.width, r.height,
        BG
    );

    Image::text(
        r.x + 20, r.y + 17,
        "SYSTEM MONITOR",
        TEXT, 2
    );

    for (uint32_t i = 0; i < 5; ++i) {
        const int32_t x =
            r.x + 18 + static_cast<int32_t>(i) * 91;

        if (x + 86 > r.x + r.width)
            break;

        Image::rectangle(
            x, r.y + 48, 86, 30,
            activePage == static_cast<Page>(i)
                ? ACTIVE : PANEL
        );

        Image::text(
            x + 6, r.y + 59,
            PAGE_NAMES[i],
            activePage == static_cast<Page>(i)
                ? TEXT : MUTED
        );
    }

    switch (activePage) {
        case Page::Overview:
            if (!available) {
                unavailable(
                    r, "No monitoring provider connected"
                );
            } else {
                Image::text(
                    r.x + 22, r.y + 110,
                    "Select CPU, Memory or Processes",
                    MUTED
                );
            }
            break;

        case Page::CPU:
            render_cpu(r);
            break;

        case Page::Memory:
            render_memory(r);
            break;

        case Page::Processes:
            render_processes(r);
            break;

        case Page::Hardware:
            render_hardware(r);
            break;
    }
}

bool click(
    const Windows::Rect& r,
    int32_t mouseX,
    int32_t mouseY
) {
    for (uint32_t i = 0; i < 5; ++i) {
        const int32_t x =
            r.x + 18 + static_cast<int32_t>(i) * 91;

        if (!contains(
            mouseX, mouseY,
            x, r.y + 48,
            86, 30
        ))
            continue;

        select(static_cast<Page>(i));
        return true;
    }

    return false;
}

const Snapshot& snapshot() {
    return current;
}

Page page() {
    return activePage;
}

bool data_available() {
    return available;
}

}
