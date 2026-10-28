#include "loading_screen.h"

#include "framebuffer.h"
#include "font.h"
#include "theme.h"

#include "../arch/x86_64/pit.h"
#include "../logging/serial.h"

#include <stdint.h>

namespace Astral::Loading {

static uint32_t totalTasks = 1;
static uint32_t completedTasks = 0;
static uint32_t failedTasks = 0;
static uint32_t shownMilliseconds = 0;

static const char* currentStage = "Preparing system";

static constexpr uint32_t PANEL_COLOR = 0x0D1733;
static constexpr uint32_t PANEL_BORDER = 0x243250;
static constexpr uint32_t BAR_BACKGROUND = 0x1A2240;

static void draw_char_scaled(
    uint32_t x,
    uint32_t y,
    char character,
    uint32_t color,
    uint32_t scale
) {
    const uint8_t* bitmap =
        Graphics::glyph(character);

    for (uint32_t row = 0; row < 7; ++row) {
        for (uint32_t col = 0; col < 5; ++col) {
            if (bitmap[row] & (1 << (4 - col))) {
                Graphics::rectangle(
                    x + col * scale,
                    y + row * scale,
                    scale,
                    scale,
                    color
                );
            }
        }
    }
}

static uint32_t text_width(
    const char* text,
    uint32_t scale
) {
    uint32_t count = 0;

    while (*text++) {
        ++count;
    }

    return count * (6 * scale);
}

static void draw_text(
    uint32_t x,
    uint32_t y,
    const char* text,
    uint32_t color,
    uint32_t scale
) {
    while (*text) {
        draw_char_scaled(x, y, *text, color, scale);
        x += 6 * scale;
        ++text;
    }
}

static void draw_number(
    uint32_t x,
    uint32_t y,
    uint32_t number,
    uint32_t color,
    uint32_t scale
) {
    char buffer[16];
    uint32_t index = 0;

    do {
        buffer[index++] =
            static_cast<char>('0' + (number % 10));
        number /= 10;
    } while (number && index < 15);

    char out[16];

    for (uint32_t i = 0; i < index; ++i) {
        out[i] = buffer[index - i - 1];
    }

    out[index] = '\0';

    draw_text(x, y, out, color, scale);
}

static void render(uint32_t etaSeconds) {
    Graphics::clear(Theme::BACKGROUND);

    uint32_t width = Graphics::width();
    uint32_t height = Graphics::height();

    uint32_t panelWidth = width > 700 ? 700 : (width - 40);
    uint32_t panelHeight = 220;

    uint32_t panelX = (width - panelWidth) / 2;
    uint32_t panelY = (height - panelHeight) / 2;

    Graphics::rectangle(
        panelX,
        panelY,
        panelWidth,
        panelHeight,
        PANEL_BORDER
    );

    Graphics::rectangle(
        panelX + 2,
        panelY + 2,
        panelWidth - 4,
        panelHeight - 4,
        PANEL_COLOR
    );

    const char* title = "ASTRALOS";
    uint32_t titleScale = 4;
    uint32_t titleX =
        (width - text_width(title, titleScale)) / 2;

    draw_text(
        titleX,
        panelY + 28,
        title,
        Theme::TEXT,
        titleScale
    );

    const char* subtitle = "INITIALIZING SYSTEM";
    uint32_t subScale = 2;
    uint32_t subX =
        (width - text_width(subtitle, subScale)) / 2;

    draw_text(
        subX,
        panelY + 80,
        subtitle,
        Theme::DOWNLOAD,
        subScale
    );

    uint32_t progress =
        (completedTasks * 100) / totalTasks;

    uint32_t barWidth = panelWidth - 120;
    uint32_t barHeight = 16;
    uint32_t barX = panelX + 60;
    uint32_t barY = panelY + 125;

    Graphics::rectangle(
        barX,
        barY,
        barWidth,
        barHeight,
        BAR_BACKGROUND
    );

    Graphics::rectangle(
        barX,
        barY,
        (barWidth * progress) / 100,
        barHeight,
        failedTasks ? Theme::WARNING : Theme::DOWNLOAD
    );

    uint32_t percentX =
        (width - text_width("100%", 2)) / 2;

    draw_number(
        percentX,
        panelY + 155,
        progress,
        Theme::TEXT,
        2
    );

    draw_text(
        percentX + text_width("100", 2),
        panelY + 155,
        "%",
        Theme::TEXT,
        2
    );

    uint32_t stageX =
        (width - text_width(currentStage, 2)) / 2;

    draw_text(
        stageX,
        panelY + 185,
        currentStage,
        failedTasks ? Theme::WARNING : Theme::SUCCESS,
        2
    );

    draw_text(
        16,
        height - 28,
        "ETA ",
        Theme::TEXT,
        2
    );

    draw_number(
        16 + text_width("ETA ", 2),
        height - 28,
        etaSeconds,
        Theme::DOWNLOAD,
        2
    );

    draw_text(
        16 + text_width("ETA ", 2) + text_width("00", 2),
        height - 28,
        "s",
        Theme::TEXT,
        2
    );
}

void begin(uint32_t tasks) {
    totalTasks = tasks ? tasks : 1;
    completedTasks = 0;
    failedTasks = 0;
    shownMilliseconds = 0;
    currentStage = "Preparing boot sequence";

    render(5);
}

void complete(const char* stage, bool success) {
    currentStage = stage ? stage : "Unknown stage";

    ++completedTasks;

    if (!success)
        ++failedTasks;

    render(5);

    // Slow visual pacing for each stage.
    if (Timer::wait_ms(700)) {
        shownMilliseconds += 700;
    }

    Serial::write(success ? "[SUCCESS] " : "[ERROR] ");
    Serial::write(currentStage);
    Serial::write("\n");
}

bool finish() {
    if (failedTasks != 0)
        return false;

    currentStage = "Finalizing startup";

    const uint32_t minimumMs = 5000;

    while (shownMilliseconds < minimumMs) {
        uint32_t remaining =
            (minimumMs - shownMilliseconds + 999) / 1000;

        render(remaining);

        if (!Timer::wait_ms(250))
            return false;

        shownMilliseconds += 250;
    }

    render(0);
    return true;
}

}