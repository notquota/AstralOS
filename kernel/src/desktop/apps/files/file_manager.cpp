
#include "file_manager.h"
#include "../../graphics/image.h"

namespace Astral::Desktop::Apps::FileManager {

namespace {

State current{};
Provider provider{};

constexpr uint32_t NONE = UINT32_MAX;

bool copy_string(
    char* destination,
    uint32_t capacity,
    const char* source
) {
    if (!destination || !source || capacity == 0)
        return false;

    uint32_t i = 0;

    while (source[i]) {
        if (i + 1 >= capacity)
            return false;
        ++i;
    }

    for (uint32_t j = 0; j <= i; ++j)
        destination[j] = source[j];

    return true;
}

uint32_t length(const char* s) {
    uint32_t n = 0;
    while (s && s[n]) ++n;
    return n;
}

bool join_path(
    char* output,
    const char* directory,
    const char* name
) {
    const uint32_t n = length(directory);
    const uint32_t m = length(name);

    const bool root =
        n == 1 && directory[0] == '/';

    const uint32_t required =
        n + (root ? 0 : 1) + m + 1;

    if (required > MAX_PATH)
        return false;

    uint32_t position = 0;

    for (uint32_t i = 0; i < n; ++i)
        output[position++] = directory[i];

    if (!root)
        output[position++] = '/';

    for (uint32_t i = 0; i < m; ++i)
        output[position++] = name[i];

    output[position] = '\0';
    return true;
}

bool inside(
    int32_t mx, int32_t my,
    int32_t x, int32_t y,
    int32_t w, int32_t h
) {
    return mx >= x && my >= y &&
           static_cast<int64_t>(mx) < static_cast<int64_t>(x) + w &&
           static_cast<int64_t>(my) < static_cast<int64_t>(y) + h;
}

void reset_listing() {
    current.entryCount = 0;
    current.selectedIndex = NONE;
    current.scrollOffset = 0;
    current.sourceAvailable = false;
}

uint32_t rows_visible(const Windows::Rect& r) {
    if (r.height <= 145)
        return 0;

    return static_cast<uint32_t>((r.height - 145) / 30);
}

}

void initialize() {
    current = {};
    copy_string(current.currentPath, MAX_PATH, "/");
    current.selectedIndex = NONE;
    current.initialized = true;
    refresh();
}

void set_provider(const Provider& value) {
    provider = value;
    if (current.initialized)
        refresh();
}

bool refresh() {
    reset_listing();

    if (!provider.list)
        return false;

    uint32_t count = 0;

    if (!provider.list(
        current.currentPath,
        current.entries,
        MAX_ENTRIES,
        count
    ))
        return false;

    if (count > MAX_ENTRIES)
        return false;

    current.entryCount = count;
    current.sourceAvailable = true;
    return true;
}

bool navigate(const char* path) {
    if (!path || path[0] != '/' || !provider.is_directory)
        return false;

    char candidate[MAX_PATH]{};

    if (!copy_string(candidate, MAX_PATH, path))
        return false;

    if (!provider.is_directory(candidate))
        return false;

    char previous[MAX_PATH]{};

    copy_string(previous, MAX_PATH, current.currentPath);
    copy_string(current.currentPath, MAX_PATH, candidate);

    if (!refresh()) {
        copy_string(current.currentPath, MAX_PATH, previous);
        refresh();
        return false;
    }

    return true;
}

bool up() {
    if (current.currentPath[0] == '/' &&
        current.currentPath[1] == '\0')
        return false;

    char parent[MAX_PATH]{};

    if (!copy_string(parent, MAX_PATH, current.currentPath))
        return false;

    uint32_t last = 0;

    for (uint32_t i = 0; parent[i]; ++i) {
        if (parent[i] == '/')
            last = i;
    }

    if (last == 0)
        parent[1] = '\0';
    else
        parent[last] = '\0';

    return navigate(parent);
}

void render(const Windows::Rect& r) {
    if (!current.initialized)
        initialize();

    Image::rectangle(
        r.x, r.y, r.width, r.height,
        0x142139
    );

    Image::rectangle(
        r.x, r.y, r.width, 46,
        0x213453
    );

    Image::rectangle(
        r.x + 12, r.y + 9, 36, 28,
        0x334A70
    );

    Image::text(r.x + 24, r.y + 19, "<", 0xFFFFFF);

    Image::text(
        r.x + 62, r.y + 19,
        current.currentPath, 0xE5EDFF
    );

    Image::text(
        r.x + 18, r.y + 65,
        "NAME", 0x9CAED0
    );

    Image::text(
        r.x + r.width - 110, r.y + 65,
        "TYPE", 0x9CAED0
    );

    Image::rectangle(
        r.x + 15, r.y + 88,
        r.width - 30, 1,
        0x3A4C69
    );

    if (!current.sourceAvailable) {
        Image::text(
            r.x + 25, r.y + 125,
            "Filesystem provider unavailable",
            0xEDB37B
        );
        return;
    }

    if (current.entryCount == 0) {
        Image::text(
            r.x + 25, r.y + 125,
            "This directory is empty",
            0xAAB9D1
        );
        return;
    }

    const uint32_t visible = rows_visible(r);

    for (uint32_t row = 0; row < visible; ++row) {
        const uint32_t index = current.scrollOffset + row;

        if (index >= current.entryCount)
            break;

        const Entry& e = current.entries[index];

        const int32_t y =
            r.y + 100 + static_cast<int32_t>(row) * 30;

        if (index == current.selectedIndex) {
            Image::rectangle(
                r.x + 13, y - 3,
                r.width - 26, 27,
                0x344C75
            );
        }

        Image::text(
            r.x + 20, y + 8,
            e.name, 0xECF1FF
        );

        Image::text(
            r.x + r.width - 105,
            y + 8,
            e.type == EntryType::Directory
                ? "Folder" : "File",
            0xA9B7D1
        );
    }
}

bool click(
    const Windows::Rect& r,
    int32_t mx,
    int32_t my
) {
    if (inside(mx, my, r.x + 12, r.y + 9, 36, 28))
        return up();

    if (!current.sourceAvailable)
        return false;

    const uint32_t visible = rows_visible(r);

    for (uint32_t row = 0; row < visible; ++row) {
        const int32_t y =
            r.y + 100 + static_cast<int32_t>(row) * 30;

        if (!inside(
            mx, my, r.x + 13, y - 3,
            r.width - 26, 27
        ))
            continue;

        const uint32_t index = current.scrollOffset + row;

        if (index >= current.entryCount)
            return false;

        if (current.selectedIndex == index &&
            current.entries[index].type == EntryType::Directory) {
            char path[MAX_PATH]{};

            if (!join_path(
                path,
                current.currentPath,
                current.entries[index].name
            ))
                return false;

            return navigate(path);
        }

        current.selectedIndex = index;
        return true;
    }

    return false;
}

void scroll(int32_t steps) {
    if (steps == 0)
        return;

    if (steps < 0) {
        const uint32_t n =
            static_cast<uint32_t>(-static_cast<int64_t>(steps));

        current.scrollOffset =
            n > current.scrollOffset
                ? 0
                : current.scrollOffset - n;
    } else {
        const uint32_t n = static_cast<uint32_t>(steps);

        const uint32_t max =
            current.entryCount > 0 ? current.entryCount - 1 : 0;

        current.scrollOffset =
            n >= max - current.scrollOffset
                ? max
                : current.scrollOffset + n;
    }
}

const State& state() {
    return current;
}

}
