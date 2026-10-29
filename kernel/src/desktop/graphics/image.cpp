
#include "image.h"

#include "../../graphics/framebuffer.h"

namespace Astral::Desktop::Image {

namespace {

// 1600x900. Yes, we're carrying a whole screen
// around in static memory. The pixels pay no rent.
constexpr uint32_t MAX_WIDTH = 1600;
constexpr uint32_t MAX_HEIGHT = 900;

static uint32_t canvas[MAX_WIDTH * MAX_HEIGHT]{};

static uint32_t canvasWidth = 0;
static uint32_t canvasHeight = 0;

static PixelReader externalReader = nullptr;

uint32_t blend(
    uint32_t background,
    uint32_t foreground,
    uint32_t alpha
) {
    if (alpha == 255)
        return foreground & 0xFFFFFF;

    if (alpha == 0)
        return background & 0xFFFFFF;

    const uint32_t inverse = 255 - alpha;

    const uint32_t red =
        (((background >> 16) & 255) * inverse +
         ((foreground >> 16) & 255) * alpha) / 255;

    const uint32_t green =
        (((background >> 8) & 255) * inverse +
         ((foreground >> 8) & 255) * alpha) / 255;

    const uint32_t blue =
        ((background & 255) * inverse +
         (foreground & 255) * alpha) / 255;

    return (red << 16) | (green << 8) | blue;
}

bool in_bounds(int32_t x, int32_t y) {
    return x >= 0 &&
           y >= 0 &&
           static_cast<uint32_t>(x) < canvasWidth &&
           static_cast<uint32_t>(y) < canvasHeight;
}

void put_pixel(
    int32_t x,
    int32_t y,
    uint32_t color
) {
    if (!in_bounds(x, y))
        return;

    canvas[
        static_cast<uint32_t>(y) * MAX_WIDTH +
        static_cast<uint32_t>(x)
    ] = color & 0xFFFFFF;
}

void put_blended_pixel(
    int32_t x,
    int32_t y,
    uint32_t color,
    uint32_t alpha
) {
    if (!in_bounds(x, y) || alpha == 0)
        return;

    const uint32_t index =
        static_cast<uint32_t>(y) * MAX_WIDTH +
        static_cast<uint32_t>(x);

    canvas[index] = blend(
        canvas[index],
        color,
        alpha
    );
}

// Tiny homemade font.
// Not exactly a typography masterpiece,
// but it works before we have a font engine.
void glyph_rows(char character, uint8_t rows[7]) {
    for (int i = 0; i < 7; ++i)
        rows[i] = 0;

    if (character >= 'a' && character <= 'z')
        character = static_cast<char>(
            character - 'a' + 'A'
        );

    static const uint8_t letters[26][7] = {
        {14,17,17,31,17,17,17}, // A
        {30,17,17,30,17,17,30}, // B
        {14,17,16,16,16,17,14}, // C
        {30,17,17,17,17,17,30}, // D
        {31,16,16,30,16,16,31}, // E
        {31,16,16,30,16,16,16}, // F
        {14,17,16,23,17,17,14}, // G
        {17,17,17,31,17,17,17}, // H
        {31,4,4,4,4,4,31},       // I
        {7,2,2,2,18,18,12},     // J
        {17,18,20,24,20,18,17}, // K
        {16,16,16,16,16,16,31}, // L
        {17,27,21,21,17,17,17}, // M
        {17,25,21,19,17,17,17}, // N
        {14,17,17,17,17,17,14}, // O
        {30,17,17,30,16,16,16}, // P
        {14,17,17,17,21,18,13}, // Q
        {30,17,17,30,20,18,17}, // R
        {15,16,16,14,1,1,30},   // S
        {31,4,4,4,4,4,4},        // T
        {17,17,17,17,17,17,14}, // U
        {17,17,17,17,17,10,4},  // V
        {17,17,17,21,21,21,10}, // W
        {17,17,10,4,10,17,17},  // X
        {17,17,10,4,4,4,4},     // Y
        {31,1,2,4,8,16,31}      // Z
    };

    static const uint8_t digits[10][7] = {
        {14,17,19,21,25,17,14}, // 0
        {4,12,4,4,4,4,14},       // 1
        {14,17,1,2,4,8,31},      // 2
        {30,1,1,14,1,1,30},      // 3
        {2,6,10,18,31,2,2},     // 4
        {31,16,16,30,1,1,30},   // 5
        {14,16,16,30,17,17,14}, // 6
        {31,1,2,4,8,8,8},        // 7
        {14,17,17,14,17,17,14}, // 8
        {14,17,17,15,1,1,14}    // 9
    };

    if (character >= 'A' && character <= 'Z') {
        for (int i = 0; i < 7; ++i)
            rows[i] = letters[character - 'A'][i];
        return;
    }

    if (character >= '0' && character <= '9') {
        for (int i = 0; i < 7; ++i)
            rows[i] = digits[character - '0'][i];
        return;
    }

    switch (character) {
        case ' ':
            break;
        case '.':
            rows[6] = 4;
            break;
        case ':':
            rows[2] = 4;
            rows[5] = 4;
            break;
        case '-':
            rows[3] = 31;
            break;
        case '_':
            rows[6] = 31;
            break;
        case '+':
            rows[2] = 4;
            rows[3] = 31;
            rows[4] = 4;
            break;
        case '/':
            rows[0] = 1;
            rows[1] = 2;
            rows[2] = 2;
            rows[3] = 4;
            rows[4] = 8;
            rows[5] = 8;
            rows[6] = 16;
            break;
        case '\\':
            rows[0] = 16;
            rows[1] = 8;
            rows[2] = 8;
            rows[3] = 4;
            rows[4] = 2;
            rows[5] = 2;
            rows[6] = 1;
            break;
        case '!':
            rows[0] = 4;
            rows[1] = 4;
            rows[2] = 4;
            rows[3] = 4;
            rows[4] = 4;
            rows[6] = 4;
            break;
        case '?':
            rows[0] = 14;
            rows[1] = 17;
            rows[2] = 1;
            rows[3] = 2;
            rows[4] = 4;
            rows[6] = 4;
            break;
        case '(':
            rows[0] = 2;
            rows[1] = 4;
            rows[2] = 8;
            rows[3] = 8;
            rows[4] = 8;
            rows[5] = 4;
            rows[6] = 2;
            break;
        case ')':
            rows[0] = 8;
            rows[1] = 4;
            rows[2] = 2;
            rows[3] = 2;
            rows[4] = 2;
            rows[5] = 4;
            rows[6] = 8;
            break;
        case '%':
            rows[0] = 17;
            rows[1] = 18;
            rows[2] = 2;
            rows[3] = 4;
            rows[4] = 8;
            rows[5] = 9;
            rows[6] = 17;
            break;
        case '=':
            rows[2] = 31;
            rows[4] = 31;
            break;
        case ',':
            rows[5] = 4;
            rows[6] = 8;
            break;
        default:
            // Unknown glyph. A box.
            // Better than silently eating the character.
            rows[0] = 31;
            rows[1] = 17;
            rows[2] = 17;
            rows[3] = 17;
            rows[4] = 17;
            rows[5] = 17;
            rows[6] = 31;
            break;
    }
}

bool clipped_bounds(
    int32_t x,
    int32_t y,
    int32_t w,
    int32_t h,
    int32_t& left,
    int32_t& top,
    int32_t& right,
    int32_t& bottom
) {
    if (canvasWidth == 0 || canvasHeight == 0 ||
        w <= 0 || h <= 0)
        return false;

    const int64_t x2 =
        static_cast<int64_t>(x) + w;
    const int64_t y2 =
        static_cast<int64_t>(y) + h;

    if (x2 <= 0 || y2 <= 0 ||
        x >= static_cast<int32_t>(canvasWidth) ||
        y >= static_cast<int32_t>(canvasHeight))
        return false;

    left = x < 0 ? 0 : x;
    top = y < 0 ? 0 : y;

    right = x2 > canvasWidth
        ? static_cast<int32_t>(canvasWidth)
        : static_cast<int32_t>(x2);

    bottom = y2 > canvasHeight
        ? static_cast<int32_t>(canvasHeight)
        : static_cast<int32_t>(y2);

    return left < right && top < bottom;
}

} // namespace

void set_pixel_reader(PixelReader reader) {
    externalReader = reader;
}

bool begin(uint32_t w, uint32_t h) {
    if (w == 0 || h == 0 ||
        w > MAX_WIDTH || h > MAX_HEIGHT)
        return false;

    canvasWidth = w;
    canvasHeight = h;

    clear(0x101827);
    return true;
}

uint32_t width() {
    return canvasWidth;
}

uint32_t height() {
    return canvasHeight;
}

void clear(uint32_t color) {
    if (!canvasWidth || !canvasHeight)
        return;

    for (uint32_t y = 0; y < canvasHeight; ++y) {
        for (uint32_t x = 0; x < canvasWidth; ++x) {
            canvas[y * MAX_WIDTH + x] =
                color & 0xFFFFFF;
        }
    }
}

void rectangle(
    int32_t x,
    int32_t y,
    int32_t w,
    int32_t h,
    uint32_t color
) {
    int32_t left, top, right, bottom;

    if (!clipped_bounds(
        x, y, w, h,
        left, top, right, bottom
    ))
        return;

    for (int32_t py = top; py < bottom; ++py) {
        for (int32_t px = left; px < right; ++px)
            put_pixel(px, py, color);
    }
}

void text(
    int32_t x,
    int32_t y,
    const char* string,
    uint32_t color,
    uint32_t scale
) {
    if (!string || scale == 0)
        return;

    if (scale > 8)
        scale = 8;

    const int32_t originX = x;
    int32_t cursorX = x;
    int32_t cursorY = y;

    // Our text uses a 5x7 bitmap font.
    // It isn't Inter, but Inter doesn't boot a kernel.
    for (uint32_t i = 0; string[i] != '\0'; ++i) {
        const char c = string[i];

        if (c == '\n') {
            cursorX = originX;
            cursorY += static_cast<int32_t>(9 * scale);
            continue;
        }

        uint8_t rows[7]{};
        glyph_rows(c, rows);

        for (int32_t gy = 0; gy < 7; ++gy) {
            for (int32_t gx = 0; gx < 5; ++gx) {
                if ((rows[gy] & (1u << (4 - gx))) == 0)
                    continue;

                rectangle(
                    cursorX + gx *
                        static_cast<int32_t>(scale),
                    cursorY + gy *
                        static_cast<int32_t>(scale),
                    static_cast<int32_t>(scale),
                    static_cast<int32_t>(scale),
                    color
                );
            }
        }

        cursorX += static_cast<int32_t>(6 * scale);

        // Prevent maliciously long text from
        // walking off into integer-overflow land.
        if (cursorX > static_cast<int32_t>(canvasWidth) + 100)
            break;
    }
}

void draw(
    const Bitmap& bitmap,
    int32_t x,
    int32_t y
) {
    if (!bitmap.rgba ||
        bitmap.width == 0 ||
        bitmap.height == 0)
        return;

    int32_t left, top, right, bottom;

    if (!clipped_bounds(
        x, y,
        static_cast<int32_t>(bitmap.width),
        static_cast<int32_t>(bitmap.height),
        left, top, right, bottom
    ))
        return;

    for (int32_t py = top; py < bottom; ++py) {
        for (int32_t px = left; px < right; ++px) {
            const uint32_t srcX =
                static_cast<uint32_t>(px - x);

            const uint32_t srcY =
                static_cast<uint32_t>(py - y);

            const uint64_t offset =
                (static_cast<uint64_t>(srcY) *
                 bitmap.width + srcX) * 4;

            const uint8_t* pixel =
                bitmap.rgba + offset;

            const uint32_t color =
                (static_cast<uint32_t>(pixel[0]) << 16) |
                (static_cast<uint32_t>(pixel[1]) << 8) |
                static_cast<uint32_t>(pixel[2]);

            put_blended_pixel(
                px, py, color, pixel[3]
            );
        }
    }
}

void draw_scaled(
    const Bitmap& bitmap,
    int32_t x,
    int32_t y,
    uint32_t w,
    uint32_t h
) {
    if (!bitmap.rgba ||
        !bitmap.width || !bitmap.height ||
        !w || !h)
        return;

    int32_t left, top, right, bottom;

    if (!clipped_bounds(
        x, y,
        static_cast<int32_t>(w),
        static_cast<int32_t>(h),
        left, top, right, bottom
    ))
        return;

    for (int32_t py = top; py < bottom; ++py) {
        for (int32_t px = left; px < right; ++px) {
            const uint32_t sx =
                static_cast<uint32_t>(
                    (static_cast<uint64_t>(px - x) *
                     bitmap.width) / w
                );

            const uint32_t sy =
                static_cast<uint32_t>(
                    (static_cast<uint64_t>(py - y) *
                     bitmap.height) / h
                );

            const uint64_t offset =
                (static_cast<uint64_t>(sy) *
                 bitmap.width + sx) * 4;

            const uint8_t* pixel =
                bitmap.rgba + offset;

            const uint32_t color =
                (static_cast<uint32_t>(pixel[0]) << 16) |
                (static_cast<uint32_t>(pixel[1]) << 8) |
                static_cast<uint32_t>(pixel[2]);

            put_blended_pixel(
                px, py, color, pixel[3]
            );
        }
    }
}

uint32_t pixel_at(int32_t x, int32_t y) {
    if (!in_bounds(x, y))
        return externalReader
            ? externalReader(x, y)
            : 0;

    return canvas[
        static_cast<uint32_t>(y) * MAX_WIDTH +
        static_cast<uint32_t>(x)
    ];
}

void present_rect(
    int32_t x,
    int32_t y,
    int32_t w,
    int32_t h
) {
    int32_t left, top, right, bottom;

    if (!clipped_bounds(
        x, y, w, h,
        left, top, right, bottom
    ))
        return;

    // Combine adjacent pixels of the same color.
    // Sending every single pixel through rectangle()
    // would make the CPU question its career.
    for (int32_t py = top; py < bottom; ++py) {
        int32_t px = left;

        while (px < right) {
            const uint32_t color =
                canvas[
                    static_cast<uint32_t>(py) * MAX_WIDTH +
                    static_cast<uint32_t>(px)
                ];

            int32_t run = 1;

            while (px + run < right &&
                canvas[
                    static_cast<uint32_t>(py) * MAX_WIDTH +
                    static_cast<uint32_t>(px + run)
                ] == color) {
                ++run;
            }

            Graphics::rectangle(
                px, py, run, 1, color
            );

            px += run;
        }
    }
}

void present() {
    present_rect(
        0, 0,
        static_cast<int32_t>(canvasWidth),
        static_cast<int32_t>(canvasHeight)
    );
}

void draw_overlay(
    const Bitmap& bitmap,
    int32_t x,
    int32_t y,
    uint32_t w,
    uint32_t h
) {
    if (!bitmap.rgba ||
        !bitmap.width || !bitmap.height ||
        !w || !h)
        return;

    int32_t left, top, right, bottom;

    if (!clipped_bounds(
        x, y,
        static_cast<int32_t>(w),
        static_cast<int32_t>(h),
        left, top, right, bottom
    ))
        return;

    // Draw directly onto the real framebuffer.
    // The software canvas stays untouched so the
    // cursor doesn't leave little ghost versions of itself.
    for (int32_t py = top; py < bottom; ++py) {
        for (int32_t px = left; px < right; ++px) {
            const uint32_t sx =
                static_cast<uint32_t>(
                    (static_cast<uint64_t>(px - x) *
                     bitmap.width) / w
                );

            const uint32_t sy =
                static_cast<uint32_t>(
                    (static_cast<uint64_t>(py - y) *
                     bitmap.height) / h
                );

            const uint64_t offset =
                (static_cast<uint64_t>(sy) *
                 bitmap.width + sx) * 4;

            const uint8_t* pixel =
                bitmap.rgba + offset;

            if (pixel[3] == 0)
                continue;

            const uint32_t color =
                (static_cast<uint32_t>(pixel[0]) << 16) |
                (static_cast<uint32_t>(pixel[1]) << 8) |
                static_cast<uint32_t>(pixel[2]);

            Graphics::rectangle(
                px, py, 1, 1,
                blend(
                    pixel_at(px, py),
                    color,
                    pixel[3]
                )
            );
        }
    }
}

} // namespace Astral::Desktop::Image
