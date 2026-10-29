
#include "terminal_app.h"

#include "../../graphics/image.h"
#include "../../core/session.h"

namespace Astral::Desktop::Apps::TerminalApp {

namespace {

State terminal{};

bool same(const char* a, const char* b) {
    if (!a || !b)
        return false;

    while (*a && *b) {
        if (*a++ != *b++)
            return false;
    }

    return *a == *b;
}

bool starts_with(const char* text, const char* prefix) {
    while (*prefix) {
        if (*text++ != *prefix++)
            return false;
    }
    return true;
}

void copy_line(char* destination, const char* source) {
    uint32_t i = 0;

    if (!source)
        source = "";

    while (source[i] && i + 1 < LINE_LENGTH) {
        destination[i] = source[i];
        ++i;
    }

    destination[i] = '\0';
}

void print(const char* text) {
    if (terminal.lineCount < MAX_LINES) {
        copy_line(
            terminal.lines[terminal.lineCount],
            text
        );
        ++terminal.lineCount;
    } else {
        for (uint32_t i = 1; i < MAX_LINES; ++i)
            copy_line(
                terminal.lines[i - 1],
                terminal.lines[i]
            );

        copy_line(
            terminal.lines[MAX_LINES - 1],
            text
        );
    }

    terminal.scrollOffset = 0;
}

void execute() {
    if (!Session::user_access_allowed()) {
        print("Permission denied: no user session");
        return;
    }

    const char* command = terminal.input;

    if (!command[0])
        return;

    if (same(command, "help")) {
        print("help, clear, whoami, version, echo");
        print("This terminal runs in User mode.");
    } else if (same(command, "clear")) {
        clear();
    } else if (same(command, "whoami")) {
        print("User session (unprivileged)");
    } else if (same(command, "version")) {
        print("AstralOS v0.0.8-alpha");
    } else if (starts_with(command, "echo ")) {
        print(command + 5);
    } else if (same(command, "echo")) {
        print("");
    } else if (
        starts_with(command, "sudo") ||
        starts_with(command, "hpt") ||
        starts_with(command, "ast ")
    ) {
        print("This command is unavailable here.");
        print("Use an authorized system interface.");
    } else {
        print("Unknown user command. Type help.");
    }
}

}

void initialize() {
    terminal = {};
    terminal.initialized = true;

    print("AstralOS Terminal");
    print("User-mode graphical terminal");
    print("Type 'help' to get started.");
    print("");
}

void clear() {
    for (uint32_t i = 0; i < MAX_LINES; ++i)
        terminal.lines[i][0] = '\0';

    terminal.lineCount = 0;
    terminal.scrollOffset = 0;
}

void handle_key(char key) {
    if (!terminal.initialized)
        initialize();

    if (!Session::user_access_allowed())
        return;

    if (key == '\r')
        key = '\n';

    if (key == '\b' || key == 127) {
        if (terminal.inputLength > 0) {
            --terminal.inputLength;
            terminal.input[terminal.inputLength] = '\0';
        }
        return;
    }

    if (key == '\n') {
        char prompt[LINE_LENGTH];

        prompt[0] = 'u';
        prompt[1] = 's';
        prompt[2] = 'e';
        prompt[3] = 'r';
        prompt[4] = '>';
        prompt[5] = ' ';

        uint32_t i = 0;
        while (i < LINE_LENGTH - 7 &&
               terminal.input[i]) {
            prompt[i + 6] = terminal.input[i];
            ++i;
        }
        prompt[i + 6] = '\0';

        print(prompt);
        execute();

        terminal.inputLength = 0;
        terminal.input[0] = '\0';
        return;
    }

    if (key >= 32 && key <= 126 &&
        terminal.inputLength + 1 < INPUT_LENGTH) {
        terminal.input[terminal.inputLength++] = key;
        terminal.input[terminal.inputLength] = '\0';
    }
}

void scroll(int32_t amount) {
    if (amount < 0) {
        uint32_t n = static_cast<uint32_t>(
            -static_cast<int64_t>(amount)
        );

        if (n > terminal.scrollOffset)
            terminal.scrollOffset = 0;
        else
            terminal.scrollOffset -= n;
    } else if (amount > 0) {
        const uint32_t n =
            static_cast<uint32_t>(amount);

        const uint32_t maxScroll =
            terminal.lineCount > 1
                ? terminal.lineCount - 1
                : 0;

        if (n >= maxScroll - terminal.scrollOffset)
            terminal.scrollOffset = maxScroll;
        else
            terminal.scrollOffset += n;
    }
}

void render(const Windows::Rect& bounds) {
    if (!terminal.initialized)
        initialize();

    Image::rectangle(
        bounds.x, bounds.y,
        bounds.width, bounds.height,
        0x0B1221
    );

    if (bounds.width < 160 || bounds.height < 80)
        return;

    const int32_t left = bounds.x + 16;
    const int32_t top = bounds.y + 16;
    const int32_t lineHeight = 17;
    const int32_t usableHeight = bounds.height - 70;

    const uint32_t visibleLines =
        usableHeight > 0
            ? static_cast<uint32_t>(
                usableHeight / lineHeight
            )
            : 0;

    uint32_t end = terminal.lineCount;

    if (terminal.scrollOffset > end)
        terminal.scrollOffset = end;

    end -= terminal.scrollOffset;

    const uint32_t start =
        end > visibleLines
            ? end - visibleLines
            : 0;

    uint32_t row = 0;

    for (uint32_t i = start; i < end; ++i) {
        Image::text(
            left,
            top + static_cast<int32_t>(
                row * lineHeight
            ),
            terminal.lines[i],
            0xD8E4F6
        );
        ++row;
    }

    Image::rectangle(
        bounds.x + 1,
        bounds.y + bounds.height - 46,
        bounds.width - 2,
        1,
        0x34415B
    );

    Image::text(
        left,
        bounds.y + bounds.height - 28,
        "user>",
        0xA993FF
    );

    // Render the visible portion of the input.
    const uint32_t maxChars =
        bounds.width > 100
            ? static_cast<uint32_t>(
                (bounds.width - 100) / 6
            )
            : 0;

    const uint32_t startInput =
        terminal.inputLength > maxChars
            ? terminal.inputLength - maxChars
            : 0;

    Image::text(
        left + 42,
        bounds.y + bounds.height - 28,
        terminal.input + startInput,
        0xFFFFFF
    );
}

const State& state() {
    return terminal;
}

}
