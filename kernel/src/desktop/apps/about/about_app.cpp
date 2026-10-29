
#include "about_app.h"
#include "../../graphics/image.h"

namespace Astral::Desktop::Apps::AboutApp {

namespace {

constexpr uint32_t BG = 0x15223B;
constexpr uint32_t PANEL = 0x20314E;
constexpr uint32_t TEXT = 0xEEF2FF;
constexpr uint32_t MUTED = 0xA5B6D0;
constexpr uint32_t ACCENT = 0xAD98FF;

Information current{
    "AstralOS",
    "v0.0.8-alpha",
    "x86_64",
    __DATE__,
    "Astral Kernel",
    "A native operating system built from scratch."
};

const Image::Bitmap* currentLogo = nullptr;

void property(
    int32_t x,
    int32_t y,
    const char* title,
    const char* value
) {
    Image::text(x, y, title, MUTED);

    Image::text(
        x + 112,
        y,
        value ? value : "Unknown",
        TEXT
    );
}

}

void initialize() {
    current = {
        "AstralOS",
        "v0.0.8-alpha",
        "x86_64",
        __DATE__,
        "Astral Kernel",
        "A native operating system built from scratch."
    };

    currentLogo = nullptr;
}

void set_information(const Information& details) {
    current = details;
}

void set_logo(const Image::Bitmap* logo) {
    currentLogo = logo;
}

void render(const Windows::Rect& r) {
    if (r.width <= 0 || r.height <= 0)
        return;

    Image::rectangle(
        r.x, r.y, r.width, r.height,
        BG
    );

    Image::rectangle(
        r.x + 18, r.y + 18,
        r.width - 36, 120,
        PANEL
    );

    if (currentLogo && currentLogo->rgba) {
        Image::draw_scaled(
            *currentLogo,
            r.x + 34,
            r.y + 35,
            83, 83
        );
    } else {
        Image::text(
            r.x + 55,
            r.y + 64,
            "A",
            ACCENT, 4
        );
    }

    Image::text(
        r.x + 135,
        r.y + 42,
        current.osName
            ? current.osName : "AstralOS",
        TEXT,
        3
    );

    Image::text(
        r.x + 135,
        r.y + 88,
        current.version
            ? current.version : "Unknown",
        ACCENT
    );

    const int32_t detailsY = r.y + 166;

    property(
        r.x + 26, detailsY,
        "Kernel", current.kernelName
    );

    property(
        r.x + 26, detailsY + 29,
        "Architecture", current.architecture
    );

    property(
        r.x + 26, detailsY + 58,
        "Build date", current.buildDate
    );

    Image::rectangle(
        r.x + 22,
        detailsY + 95,
        r.width - 44,
        1,
        0x364766
    );

    if (current.description) {
        Image::text(
            r.x + 26,
            detailsY + 117,
            current.description,
            MUTED
        );
    }
}

const Information& information() {
    return current;
}

}
