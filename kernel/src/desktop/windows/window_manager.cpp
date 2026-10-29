
#include "window_manager.h"

namespace Astral::Desktop::Windows {

namespace {

Window windows[MAX_WINDOWS]{};

uint32_t windowCount = 0;
WindowId nextId = 1;
uint32_t nextZ = 1;

WindowId focusedId = INVALID_WINDOW;

uint32_t displayWidth = 800;
uint32_t displayHeight = 600;
uint32_t reservedTaskbar = 52;

bool dragActive = false;
WindowId draggedId = INVALID_WINDOW;

int32_t dragStartX = 0;
int32_t dragStartY = 0;

int32_t windowStartX = 0;
int32_t windowStartY = 0;

void copy_title(char* output, const char* input) {
    if (!output)
        return;

    if (!input)
        input = "Untitled";

    uint32_t i = 0;

    while (input[i] && i < 63) {
        output[i] = input[i];
        ++i;
    }

    output[i] = '\0';
}

Window* find(WindowId id) {
    if (id == INVALID_WINDOW)
        return nullptr;

    for (uint32_t i = 0; i < windowCount; ++i) {
        if (windows[i].id == id)
            return &windows[i];
    }

    return nullptr;
}

void update_focus(WindowId id) {
    focusedId = id;

    for (uint32_t i = 0; i < windowCount; ++i)
        windows[i].focused =
            windows[i].id == id;
}

WindowId highest_visible() {
    WindowId result = INVALID_WINDOW;
    uint32_t highest = 0;

    for (uint32_t i = 0; i < windowCount; ++i) {
        const Window& window = windows[i];

        if (!window.visible ||
            window.state == WindowState::Minimized)
            continue;

        if (result == INVALID_WINDOW ||
            window.zOrder > highest) {
            highest = window.zOrder;
            result = window.id;
        }
    }

    return result;
}

void compact_z_order() {
    uint32_t assigned = 0;
    bool used[MAX_WINDOWS]{};

    for (uint32_t pass = 0;
         pass < windowCount;
         ++pass) {
        uint32_t best = MAX_WINDOWS;

        for (uint32_t i = 0; i < windowCount; ++i) {
            if (used[i])
                continue;

            if (best == MAX_WINDOWS ||
                windows[i].zOrder <
                windows[best].zOrder)
                best = i;
        }

        if (best == MAX_WINDOWS)
            break;

        used[best] = true;
        windows[best].zOrder = ++assigned;
    }

    nextZ = assigned + 1;
}

void clamp_position(Window& window) {
    int32_t maxX =
        static_cast<int32_t>(displayWidth) - 80;

    int32_t maxY =
        static_cast<int32_t>(
            displayHeight - reservedTaskbar
        ) - 38;

    if (maxX < 0) maxX = 0;
    if (maxY < 0) maxY = 0;

    // Keep at least some of the title bar visible.
    if (window.bounds.x > maxX)
        window.bounds.x = maxX;

    if (window.bounds.x <
        80 - window.bounds.width)
        window.bounds.x =
            80 - window.bounds.width;

    if (window.bounds.y < 0)
        window.bounds.y = 0;

    if (window.bounds.y > maxY)
        window.bounds.y = maxY;
}

}

void initialize(
    uint32_t screenWidth,
    uint32_t screenHeight,
    uint32_t taskbarHeight
) {
    for (uint32_t i = 0; i < MAX_WINDOWS; ++i)
        windows[i] = {};

    windowCount = 0;
    nextId = 1;
    nextZ = 1;
    focusedId = INVALID_WINDOW;

    dragActive = false;
    draggedId = INVALID_WINDOW;

    displayWidth = screenWidth;
    displayHeight = screenHeight;
    reservedTaskbar = taskbarHeight;
}

WindowId create(
    uint32_t appId,
    const char* title,
    const Rect& bounds,
    WindowType type
) {
    if (windowCount >= MAX_WINDOWS)
        return INVALID_WINDOW;

    if (bounds.width <= 0 || bounds.height <= 0)
        return INVALID_WINDOW;

    if (nextId == INVALID_WINDOW)
        return INVALID_WINDOW;

    Window& window = windows[windowCount++];

    window = {};

    window.id = nextId++;
    window.appId = appId;

    copy_title(window.title, title);

    window.bounds = bounds;
    window.restoredBounds = bounds;

    window.type = type;
    window.state = WindowState::Normal;

    window.visible = true;
    window.focused = false;

    window.closable = true;
    window.resizable = true;
    window.movable = true;
    window.modal = type == WindowType::Dialog;

    clamp_position(window);

    if (nextZ == UINT32_MAX)
        compact_z_order();

    window.zOrder = nextZ++;

    focus(window.id);

    return window.id;
}

bool close(WindowId id) {
    uint32_t index = MAX_WINDOWS;

    for (uint32_t i = 0; i < windowCount; ++i) {
        if (windows[i].id == id) {
            index = i;
            break;
        }
    }

    if (index == MAX_WINDOWS)
        return false;

    if (!windows[index].closable)
        return false;

    if (draggedId == id)
        end_drag();

    for (uint32_t i = index; i + 1 < windowCount; ++i)
        windows[i] = windows[i + 1];

    --windowCount;
    windows[windowCount] = {};

    if (focusedId == id)
        update_focus(highest_visible());

    return true;
}

bool focus(WindowId id) {
    Window* window = find(id);

    if (!window ||
        !window->visible ||
        window->state == WindowState::Minimized)
        return false;

    if (nextZ == UINT32_MAX)
        compact_z_order();

    window->zOrder = nextZ++;
    update_focus(id);

    return true;
}

bool minimize(WindowId id) {
    Window* window = find(id);

    if (!window)
        return false;

    window->state = WindowState::Minimized;

    if (focusedId == id)
        update_focus(highest_visible());

    return true;
}

bool maximize(WindowId id) {
    Window* window = find(id);

    if (!window || !window->resizable)
        return false;

    if (window->state != WindowState::Maximized)
        window->restoredBounds = window->bounds;

    window->bounds = {
        0,
        0,
        static_cast<int32_t>(displayWidth),
        static_cast<int32_t>(
            displayHeight > reservedTaskbar
                ? displayHeight - reservedTaskbar
                : 0
        )
    };

    window->state = WindowState::Maximized;
    return focus(id);
}

bool restore(WindowId id) {
    Window* window = find(id);

    if (!window)
        return false;

    if (window->state == WindowState::Maximized)
        window->bounds = window->restoredBounds;

    window->state = WindowState::Normal;
    return focus(id);
}

bool move(WindowId id, int32_t x, int32_t y) {
    Window* window = find(id);

    if (!window ||
        !window->movable ||
        window->state != WindowState::Normal)
        return false;

    window->bounds.x = x;
    window->bounds.y = y;
    clamp_position(*window);

    return true;
}

bool resize(
    WindowId id,
    int32_t width,
    int32_t height
) {
    Window* window = find(id);

    if (!window ||
        !window->resizable ||
        window->state != WindowState::Normal)
        return false;

    if (width < 240 || height < 160)
        return false;

    window->bounds.width = width;
    window->bounds.height = height;

    clamp_position(*window);

    return true;
}

Window* get(WindowId id) {
    return find(id);
}

const Window* at(uint32_t index) {
    return index < windowCount
        ? &windows[index]
        : nullptr;
}

uint32_t count() {
    return windowCount;
}

WindowId focused() {
    return focusedId;
}

WindowId hit_test(int32_t x, int32_t y) {
    WindowId result = INVALID_WINDOW;
    uint32_t highest = 0;

    for (uint32_t i = 0; i < windowCount; ++i) {
        const Window& window = windows[i];

        if (!window.visible ||
            window.state == WindowState::Minimized)
            continue;

        if (!contains(window.bounds, x, y))
            continue;

        if (result == INVALID_WINDOW ||
            window.zOrder > highest) {
            highest = window.zOrder;
            result = window.id;
        }
    }

    return result;
}

bool begin_drag(
    WindowId id,
    int32_t x,
    int32_t y
) {
    Window* window = find(id);

    if (!window ||
        !window->movable ||
        window->state != WindowState::Normal)
        return false;

    Rect titlebar = {
        window->bounds.x,
        window->bounds.y,
        window->bounds.width,
        38
    };

    if (!contains(titlebar, x, y))
        return false;

    focus(id);

    draggedId = id;
    dragActive = true;

    dragStartX = x;
    dragStartY = y;

    windowStartX = window->bounds.x;
    windowStartY = window->bounds.y;

    return true;
}

bool drag_to(int32_t x, int32_t y) {
    if (!dragActive)
        return false;

    return move(
        draggedId,
        windowStartX + (x - dragStartX),
        windowStartY + (y - dragStartY)
    );
}

void end_drag() {
    dragActive = false;
    draggedId = INVALID_WINDOW;
}

bool dragging() {
    return dragActive;
}

}
