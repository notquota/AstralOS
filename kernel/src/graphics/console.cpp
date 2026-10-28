
#include "console.h"
#include "framebuffer.h"
#include "font.h"
#include "theme.h"

#include <stdint.h>

namespace Astral::Console {

static constexpr uint32_t MAX_LINES = 512;
static constexpr uint32_t MAX_COLUMNS = 160;

static constexpr uint32_t CHAR_WIDTH = 8;
static constexpr uint32_t CHAR_HEIGHT = 12;

static constexpr uint32_t MARGIN_X = 16;
static constexpr uint32_t MARGIN_Y = 16;

struct Cell {
    char character;
    uint32_t color;
};

struct Line {
    Cell cells[MAX_COLUMNS];
    uint32_t length;
};

static Line history[MAX_LINES]{};

static uint64_t firstLine = 0;
static uint64_t currentLine = 0;

static uint32_t cursorColumn = 0;
static uint32_t scrollOffset = 0;

static bool ready = false;

static uint32_t columns() {
    uint32_t width = Graphics::width();

    if (width <= MARGIN_X * 2 + CHAR_WIDTH)
        return 1;

    uint32_t count =
        (width - MARGIN_X * 2) / CHAR_WIDTH;

    return count > MAX_COLUMNS
        ? MAX_COLUMNS : count;
}

static uint32_t visible_lines() {
    uint32_t height = Graphics::height();

    if (height <= MARGIN_Y * 2 + CHAR_HEIGHT)
        return 1;

    return (height - MARGIN_Y * 2) / CHAR_HEIGHT;
}

static Line& get_line(uint64_t index) {
    return history[index % MAX_LINES];
}

static void reset_line(uint64_t index) {
    Line& line = get_line(index);
    line.length = 0;
}

static uint64_t newest_top() {
    uint32_t visible = visible_lines();

    if (currentLine + 1 <= visible)
        return firstLine;

    uint64_t top = currentLine + 1 - visible;

    return top < firstLine ? firstLine : top;
}

static uint64_t current_top() {
    uint64_t bottomTop = newest_top();

    if (scrollOffset > bottomTop - firstLine)
        return firstLine;

    return bottomTop - scrollOffset;
}

static void draw_character(
    uint32_t x,
    uint32_t y,
    char character,
    uint32_t color
) {
    const uint8_t* bitmap =
        Graphics::glyph(character);

    for (uint32_t row = 0; row < 7; ++row) {
        for (uint32_t col = 0; col < 5; ++col) {
            if (bitmap[row] & (1u << (4 - col))) {
                Graphics::rectangle(
                    x + col,
                    y + row,
                    1,
                    1,
                    color
                );
            }
        }
    }
}

static void render() {
    if (!ready)
        return;

    Graphics::clear(Theme::BACKGROUND);

    uint64_t top = current_top();
    uint32_t visible = visible_lines();
    uint32_t width = columns();

    for (uint32_t row = 0; row < visible; ++row) {
        uint64_t index = top + row;

        if (index > currentLine)
            break;

        Line& line = get_line(index);

        uint32_t count = line.length;

        if (count > width)
            count = width;

        for (uint32_t col = 0; col < count; ++col) {
            Cell& cell = line.cells[col];

            draw_character(
                MARGIN_X + col * CHAR_WIDTH,
                MARGIN_Y + row * CHAR_HEIGHT,
                cell.character,
                cell.color
            );
        }
    }

    // Visual scroll indicator.
    if (scrollOffset > 0) {
        uint32_t screenWidth = Graphics::width();

        if (screenWidth >= 12) {
            Graphics::rectangle(
                screenWidth - 7,
                MARGIN_Y,
                3,
                28,
                Theme::DOWNLOAD
            );
        }
    }
}

static void advance_line() {
    ++currentLine;
    cursorColumn = 0;

    if (currentLine - firstLine >= MAX_LINES)
        firstLine = currentLine - MAX_LINES + 1;

    reset_line(currentLine);
}

void initialize() {
    firstLine = 0;
    currentLine = 0;
    cursorColumn = 0;
    scrollOffset = 0;

    // Old line contents can remain in memory.
    // Only active lines will be displayed.
    reset_line(0);

    ready = true;
    render();
}

void newline() {
    if (!ready)
        return;

    advance_line();

    // New output brings us back to the live prompt.
    scrollOffset = 0;

    render();
}

void write(
    const char* message,
    uint32_t color
) {
    if (!ready || !message)
        return;

    bool changed = false;

    while (*message) {
        char c = *message++;

        if (c == '\n') {
            advance_line();
            changed = true;
            continue;
        }

        if (c == '\r') {
            cursorColumn = 0;
            changed = true;
            continue;
        }

        if (c == '\t') {
            uint32_t spaces = 4 - (cursorColumn % 4);

            while (spaces--) {
                if (cursorColumn >= columns())
                    advance_line();

                Line& line = get_line(currentLine);

                line.cells[cursorColumn] = {
                    ' ',
                    color
                };

                ++cursorColumn;

                if (cursorColumn > line.length)
                    line.length = cursorColumn;
            }

            changed = true;
            continue;
        }

        if (cursorColumn >= columns())
            advance_line();

        Line& line = get_line(currentLine);

        line.cells[cursorColumn] = {
            c,
            color
        };

        ++cursorColumn;

        if (cursorColumn > line.length)
            line.length = cursorColumn;

        changed = true;
    }

    if (changed) {
        scrollOffset = 0;
        render();
    }
}

void backspace() {
    if (!ready || cursorColumn == 0)
        return;

    --cursorColumn;

    Line& line = get_line(currentLine);

    if (line.length > cursorColumn)
        line.length = cursorColumn;

    scrollOffset = 0;
    render();
}

void scroll_lines(int32_t lines) {
    if (!ready || lines == 0)
        return;

    uint64_t maximum = newest_top() - firstLine;

    if (lines < 0) {
        uint64_t amount =
            static_cast<uint64_t>(-(int64_t)lines);

        uint64_t remaining =
            maximum - (scrollOffset > maximum
                ? maximum : scrollOffset);

        if (amount > remaining)
            amount = remaining;

        scrollOffset += static_cast<uint32_t>(amount);
    } else {
        uint32_t amount = static_cast<uint32_t>(lines);

        if (amount >= scrollOffset)
            scrollOffset = 0;
        else
            scrollOffset -= amount;
    }

    render();
}

void scroll_page_up() {
    scroll_lines(
        -static_cast<int32_t>(visible_lines())
    );
}

void scroll_page_down() {
    scroll_lines(
        static_cast<int32_t>(visible_lines())
    );
}

void scroll_to_bottom() {
    if (!ready)
        return;

    scrollOffset = 0;
    render();
}

bool is_scrolled() {
    return scrollOffset != 0;
}

}