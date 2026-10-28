
#include "compositor.h"

#include <stdint.h>

namespace Astral::Compositor {

static bool ready = false;

static volatile uint8_t* front = nullptr;
static uint32_t pitch = 0;

static Surface back{};

static const uint32_t* cursorPixels = nullptr;

static uint32_t cursorWidth = 0;
static uint32_t cursorHeight = 0;

static int32_t cursorX = 0;
static int32_t cursorY = 0;

static bool cursorVisible = false;

struct Bounds {
    uint32_t left;
    uint32_t top;
    uint32_t right;
    uint32_t bottom;
};

static bool clip_rectangle(
    Rectangle rect,
    Bounds& result
) {
    if (!ready ||
        rect.width == 0 ||
        rect.height == 0)
        return false;

    int64_t x0 = rect.x;
    int64_t y0 = rect.y;

    int64_t x1 = x0 + rect.width;
    int64_t y1 = y0 + rect.height;

    if (x1 <= 0 || y1 <= 0)
        return false;

    if (x0 >= back.width ||
        y0 >= back.height)
        return false;

    if (x0 < 0)
        x0 = 0;

    if (y0 < 0)
        y0 = 0;

    if (x1 > back.width)
        x1 = back.width;

    if (y1 > back.height)
        y1 = back.height;

    if (x0 >= x1 || y0 >= y1)
        return false;

    result.left = static_cast<uint32_t>(x0);
    result.top = static_cast<uint32_t>(y0);
    result.right = static_cast<uint32_t>(x1);
    result.bottom = static_cast<uint32_t>(y1);

    return true;
}

static uint32_t* back_at(
    uint32_t x,
    uint32_t y
) {
    return back.pixels +
        static_cast<uint64_t>(y) *
        back.stridePixels + x;
}

static volatile uint32_t* front_at(
    uint32_t x,
    uint32_t y
) {
    return reinterpret_cast<volatile uint32_t*>(
        front +
        static_cast<uint64_t>(y) * pitch +
        static_cast<uint64_t>(x) * 4
    );
}

static uint32_t blend(
    uint32_t background,
    uint32_t foreground
) {
    const uint32_t alpha =
        foreground & 0xFFu;

    if (alpha == 0)
        return background;

    uint32_t r = (foreground >> 24) & 0xFFu;
    uint32_t g = (foreground >> 16) & 0xFFu;
    uint32_t b = (foreground >> 8) & 0xFFu;

    if (alpha == 255)
        return (r << 16) | (g << 8) | b;

    const uint32_t br =
        (background >> 16) & 0xFFu;

    const uint32_t bg =
        (background >> 8) & 0xFFu;

    const uint32_t bb =
        background & 0xFFu;

    const uint32_t inverse = 255 - alpha;

    r = (r * alpha + br * inverse + 127) / 255;
    g = (g * alpha + bg * inverse + 127) / 255;
    b = (b * alpha + bb * inverse + 127) / 255;

    return (r << 16) | (g << 8) | b;
}

static void copy_region(Bounds bounds) {
    for (uint32_t y = bounds.top;
         y < bounds.bottom;
         ++y) {
        for (uint32_t x = bounds.left;
             x < bounds.right;
             ++x) {
            *front_at(x, y) = *back_at(x, y);
        }
    }
}

static bool cursor_intersects(Bounds region) {
    if (!cursorVisible || !cursorPixels)
        return false;

    const int64_t x0 = cursorX;
    const int64_t y0 = cursorY;

    const int64_t x1 = x0 + cursorWidth;
    const int64_t y1 = y0 + cursorHeight;

    return x0 < region.right &&
           x1 > region.left &&
           y0 < region.bottom &&
           y1 > region.top;
}

static void draw_cursor_clipped(Bounds region) {
    if (!cursorVisible || !cursorPixels)
        return;

    const int64_t startX = cursorX < 0 ? 0 : cursorX;
    const int64_t startY = cursorY < 0 ? 0 : cursorY;

    int64_t endX =
        static_cast<int64_t>(cursorX) +
        cursorWidth;

    int64_t endY =
        static_cast<int64_t>(cursorY) +
        cursorHeight;

    if (endX > back.width)
        endX = back.width;

    if (endY > back.height)
        endY = back.height;

    for (int64_t y = startY; y < endY; ++y) {
        if (y < region.top || y >= region.bottom)
            continue;

        for (int64_t x = startX; x < endX; ++x) {
            if (x < region.left || x >= region.right)
                continue;

            const uint64_t sx =
                static_cast<uint64_t>(x - cursorX);

            const uint64_t sy =
                static_cast<uint64_t>(y - cursorY);

            const uint32_t foreground =
                cursorPixels[
                    sy * cursorWidth + sx
                ];

            const uint32_t ux =
                static_cast<uint32_t>(x);

            const uint32_t uy =
                static_cast<uint32_t>(y);

            *front_at(ux, uy) = blend(
                *back_at(ux, uy),
                foreground
            );
        }
    }
}

static void present_region(Rectangle rect) {
    Bounds bounds{};

    if (!clip_rectangle(rect, bounds))
        return;

    copy_region(bounds);

    if (cursor_intersects(bounds))
        draw_cursor_clipped(bounds);
}

bool initialize(
    volatile void* framebuffer,
    uint32_t width,
    uint32_t height,
    uint32_t pitchBytes,
    Allocator allocator
) {
    if (ready)
        return false;

    if (!framebuffer ||
        !allocator ||
        width == 0 ||
        height == 0)
        return false;

    if (width > UINT32_MAX / 4)
        return false;

    if (pitchBytes < width * 4 ||
        (pitchBytes & 3u) != 0)
        return false;

    const uint64_t pixelCount =
        static_cast<uint64_t>(width) * height;

    if (pixelCount >
        UINT64_MAX / sizeof(uint32_t))
        return false;

    const uint64_t bytes =
        pixelCount * sizeof(uint32_t);

    void* allocation =
        allocator(bytes, alignof(uint32_t));

    if (!allocation)
        return false;

    if ((reinterpret_cast<uint64_t>(allocation) &
         (alignof(uint32_t) - 1)) != 0)
        return false;

    front = reinterpret_cast<volatile uint8_t*>(
        framebuffer
    );

    pitch = pitchBytes;

    back.pixels =
        static_cast<uint32_t*>(allocation);

    back.width = width;
    back.height = height;
    back.stridePixels = width;

    cursorPixels = nullptr;
    cursorWidth = 0;
    cursorHeight = 0;
    cursorVisible = false;
    cursorX = 0;
    cursorY = 0;

    ready = true;

    // Preserve the current framebuffer
    // appearance when compositor starts.
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            *back_at(x, y) = *front_at(x, y);
        }
    }

    return true;
}

bool initialized() {
    return ready;
}

Surface backbuffer() {
    return back;
}

void clear(uint32_t color) {
    if (!ready)
        return;

    const uint64_t count =
        static_cast<uint64_t>(back.width) *
        back.height;

    for (uint64_t i = 0; i < count; ++i)
        back.pixels[i] = color;
}

void pixel(
    int32_t x,
    int32_t y,
    uint32_t color
) {
    if (!ready ||
        x < 0 ||
        y < 0 ||
        static_cast<uint32_t>(x) >= back.width ||
        static_cast<uint32_t>(y) >= back.height)
        return;

    *back_at(
        static_cast<uint32_t>(x),
        static_cast<uint32_t>(y)
    ) = color;
}

void fill(
    Rectangle rect,
    uint32_t color
) {
    Bounds bounds{};

    if (!clip_rectangle(rect, bounds))
        return;

    for (uint32_t y = bounds.top;
         y < bounds.bottom;
         ++y) {
        for (uint32_t x = bounds.left;
             x < bounds.right;
             ++x) {
            *back_at(x, y) = color;
        }
    }
}

void present() {
    if (!ready)
        return;

    present_region({
        0,
        0,
        back.width,
        back.height
    });
}

void present_rect(Rectangle rect) {
    present_region(rect);
}

void hide_cursor() {
    if (!ready || !cursorVisible)
        return;

    const Rectangle old = {
        cursorX,
        cursorY,
        cursorWidth,
        cursorHeight
    };

    cursorVisible = false;
    present_region(old);
}

void set_cursor(
    const uint32_t* pixels,
    uint32_t width,
    uint32_t height
) {
    if (!ready)
        return;

    hide_cursor();

    cursorPixels = pixels;
    cursorWidth = width;
    cursorHeight = height;
}

void move_cursor(
    int32_t x,
    int32_t y
) {
    if (!ready ||
        !cursorPixels ||
        cursorWidth == 0 ||
        cursorHeight == 0)
        return;

    if (cursorVisible &&
        x == cursorX &&
        y == cursorY)
        return;

    hide_cursor();

    cursorX = x;
    cursorY = y;
    cursorVisible = true;

    Bounds region{};

    if (clip_rectangle({
        cursorX,
        cursorY,
        cursorWidth,
        cursorHeight
    }, region)) {
        draw_cursor_clipped(region);
    }
}

}
