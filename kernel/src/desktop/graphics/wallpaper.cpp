
#include "wallpaper.h"

namespace Astral::Desktop::Wallpaper {

namespace {

const Image::Bitmap* currentBitmap = nullptr;

Configuration current{
    Mode::Fill,
    0x091225,
    true
};

void fallback_gradient(
    uint32_t width,
    uint32_t height
) {
    if (!width || !height)
        return;

    const uint32_t denominator =
        height > 1 ? height - 1 : 1;

    for (uint32_t y = 0; y < height; ++y) {
        const uint32_t r = 8 + y * 4 / denominator;
        const uint32_t g = 13 + y * 10 / denominator;
        const uint32_t b = 31 + y * 29 / denominator;

        const uint32_t color =
            (r << 16) | (g << 8) | b;

        Image::rectangle(
            0,
            static_cast<int32_t>(y),
            static_cast<int32_t>(width),
            1,
            color
        );
    }
}

void fit(
    const Image::Bitmap& bitmap,
    uint32_t width,
    uint32_t height
) {
    if (!bitmap.width || !bitmap.height)
        return;

    const uint64_t scaleX =
        static_cast<uint64_t>(width) * 100000 /
        bitmap.width;

    const uint64_t scaleY =
        static_cast<uint64_t>(height) * 100000 /
        bitmap.height;

    const uint64_t scale =
        scaleX < scaleY ? scaleX : scaleY;

    const uint32_t drawW = static_cast<uint32_t>(
        static_cast<uint64_t>(bitmap.width) *
        scale / 100000
    );

    const uint32_t drawH = static_cast<uint32_t>(
        static_cast<uint64_t>(bitmap.height) *
        scale / 100000
    );

    if (!drawW || !drawH)
        return;

    Image::draw_scaled(
        bitmap,
        static_cast<int32_t>((width - drawW) / 2),
        static_cast<int32_t>((height - drawH) / 2),
        drawW, drawH
    );
}

void tile(
    const Image::Bitmap& bitmap,
    uint32_t width,
    uint32_t height
) {
    if (!bitmap.width || !bitmap.height)
        return;

    for (uint32_t y = 0; y < height; y += bitmap.height) {
        for (uint32_t x = 0; x < width; x += bitmap.width) {
            Image::draw(
                bitmap,
                static_cast<int32_t>(x),
                static_cast<int32_t>(y)
            );
        }
    }
}

}

void initialize() {
    currentBitmap = nullptr;

    current = {
        Mode::Fill,
        0x091225,
        true
    };
}

void set_bitmap(const Image::Bitmap* bitmap) {
    currentBitmap = bitmap;
}

void set_configuration(const Configuration& config) {
    current = config;
}

const Configuration& configuration() {
    return current;
}

void render(
    uint32_t width,
    uint32_t height
) {
    Image::clear(current.backgroundColor);

    if (!current.enabled) {
        fallback_gradient(width, height);
        return;
    }

    if (!currentBitmap ||
        !currentBitmap->rgba ||
        !currentBitmap->width ||
        !currentBitmap->height) {
        fallback_gradient(width, height);
        return;
    }

    const Image::Bitmap& bitmap = *currentBitmap;

    switch (current.mode) {
        case Mode::Fill:
            Image::draw_scaled(
                bitmap, 0, 0,
                width, height
            );
            break;

        case Mode::Fit:
            fit(bitmap, width, height);
            break;

        case Mode::Center:
            Image::draw(
                bitmap,
                static_cast<int32_t>(
                    (static_cast<int64_t>(width) -
                     bitmap.width) / 2
                ),
                static_cast<int32_t>(
                    (static_cast<int64_t>(height) -
                     bitmap.height) / 2
                )
            );
            break;

        case Mode::Tile:
            tile(bitmap, width, height);
            break;
    }
}

}
