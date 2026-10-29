#include "browser_app.h"
#include "../../graphics/image.h"

namespace Astral::Desktop::Apps::BrowserApp {

namespace {

constexpr uint32_t WHITE = 0xEDF2FF;
constexpr uint32_t MUTED = 0xA9B8D0;
constexpr uint32_t BACKGROUND = 0x142139;
constexpr uint32_t TOOLBAR = 0x21324D;
constexpr uint32_t INPUT_BG = 0x101A2E;
constexpr uint32_t PURPLE = 0xA38BFF;

constexpr uint32_t URL_CAPACITY = 192;
constexpr uint32_t HISTORY_CAPACITY = 12;

Plugin activePlugin{};
PageState state = PageState::Home;

char address[URL_CAPACITY]{};
char history[HISTORY_CAPACITY][URL_CAPACITY]{};

uint32_t historyCount = 0;
uint32_t historyIndex = 0;

bool addressFocused = false;

void copy_string(
    char* destination,
    const char* source,
    uint32_t capacity
) {
    if (!destination || !capacity)
        return;

    uint32_t i = 0;

    if (source) {
        while (source[i] && i + 1 < capacity) {
            destination[i] = source[i];
            ++i;
        }
    }

    destination[i] = '\0';
}

uint32_t length(const char* text) {
    uint32_t i = 0;

    while (text && text[i] && i < URL_CAPACITY)
        ++i;

    return i;
}

bool inside(
    const Windows::Rect& r,
    int32_t x,
    int32_t y
) {
    return Windows::contains(r, x, y);
}

void button(
    const Windows::Rect& r,
    const char* label
) {
    Image::rectangle(
        r.x, r.y,
        r.width, r.height,
        0x304360
    );

    Image::text(
        r.x + 9,
        r.y + 13,
        label,
        WHITE
    );
}

void add_history(const char* url) {
    if (historyCount &&
        historyIndex + 1 < historyCount) {
        historyCount = historyIndex + 1;
    }

    if (historyCount >= HISTORY_CAPACITY) {
        for (uint32_t i = 1;
             i < HISTORY_CAPACITY;
             ++i) {
            copy_string(
                history[i - 1],
                history[i],
                URL_CAPACITY
            );
        }

        historyCount = HISTORY_CAPACITY - 1;
    }

    copy_string(
        history[historyCount],
        url,
        URL_CAPACITY
    );

    historyIndex = historyCount;
    ++historyCount;
}

bool visit(
    const char* url,
    bool saveHistory
) {
    if (!url || !url[0])
        return false;

    copy_string(address, url, URL_CAPACITY);

    if (!activePlugin.navigate) {
        state = PageState::PluginMissing;
        return false;
    }

    if (!activePlugin.online ||
        !activePlugin.online()) {
        state = PageState::Offline;
        return false;
    }

    state = PageState::Loading;

    if (!activePlugin.navigate(address)) {
        state = PageState::Error;
        return false;
    }

    if (saveHistory)
        add_history(address);

    state = PageState::Loaded;
    return true;
}

} // namespace

void initialize() {
    // New window, same existential crisis.
    address[0] = '\0';
    historyCount = 0;
    historyIndex = 0;
    addressFocused = false;
    state = PageState::Home;
}

void set_plugin(const Plugin& plugin) {
    activePlugin = plugin;
}

bool plugin_available() {
    return activePlugin.navigate != nullptr &&
           activePlugin.render != nullptr &&
           activePlugin.online != nullptr;
}

bool navigate(const char* url) {
    return visit(url, true);
}

bool back() {
    if (historyCount == 0 ||
        historyIndex == 0)
        return false;

    --historyIndex;

    return visit(
        history[historyIndex],
        false
    );
}

bool forward() {
    if (historyCount == 0 ||
        historyIndex + 1 >= historyCount)
        return false;

    ++historyIndex;

    return visit(
        history[historyIndex],
        false
    );
}

void reload() {
    if (activePlugin.reload &&
        activePlugin.online &&
        activePlugin.online()) {
        activePlugin.reload();
        return;
    }

    if (address[0])
        visit(address, false);
}

void poll() {
    if (activePlugin.poll)
        activePlugin.poll();
}

const char* current_url() {
    return address;
}

PageState page_state() {
    return state;
}

bool handle_key(char key) {
    if (!addressFocused)
        return false;

    uint32_t n = length(address);

    if (key == '\b') {
        if (n)
            address[n - 1] = '\0';

        return true;
    }

    if (key == '\n' || key == '\r') {
        addressFocused = false;

        // The Enter key has requested the internet.
        // The network stack hasn't RSVP'd.
        navigate(address);
        return true;
    }

    if (key >= 32 && key <= 126 &&
        n + 1 < URL_CAPACITY) {

        address[n] = key;
        address[n + 1] = '\0';
        return true;
    }

    return false;
}

bool click(
    const Windows::Rect& area,
    int32_t x,
    int32_t y
) {
    if (!inside(area, x, y))
        return false;

    const int32_t top = area.y + 12;

    Windows::Rect backButton{
        area.x + 12, top, 42, 34
    };

    Windows::Rect forwardButton{
        area.x + 60, top, 42, 34
    };

    Windows::Rect reloadButton{
        area.x + 108, top, 42, 34
    };

    Windows::Rect urlBox{
        area.x + 160,
        top,
        area.width - 172,
        34
    };

    if (inside(backButton, x, y)) {
        addressFocused = false;
        back();
        return true;
    }

    if (inside(forwardButton, x, y)) {
        addressFocused = false;
        forward();
        return true;
    }

    if (inside(reloadButton, x, y)) {
        addressFocused = false;
        reload();
        return true;
    }

    addressFocused = inside(urlBox, x, y);

    return true;
}

void render(const Windows::Rect& area) {
    if (area.width <= 0 || area.height <= 0)
        return;

    Image::rectangle(
        area.x, area.y,
        area.width, area.height,
        BACKGROUND
    );

    Image::rectangle(
        area.x, area.y,
        area.width, 60,
        TOOLBAR
    );

    const int32_t top = area.y + 12;

    button(
        {area.x + 12, top, 42, 34},
        "<"
    );

    button(
        {area.x + 60, top, 42, 34},
        ">"
    );

    button(
        {area.x + 108, top, 42, 34},
        "R"
    );

    const Windows::Rect urlBox{
        area.x + 160,
        top,
        area.width - 172,
        34
    };

    Image::rectangle(
        urlBox.x,
        urlBox.y,
        urlBox.width,
        urlBox.height,
        addressFocused ? 0x34456A : INPUT_BG
    );

    Image::text(
        urlBox.x + 10,
        urlBox.y + 13,
        address[0]
            ? address
            : "Enter a URL...",
        address[0] ? WHITE : MUTED
    );

    Windows::Rect content{
        area.x + 12,
        area.y + 72,
        area.width - 24,
        area.height - 84
    };

    if (content.width <= 0 ||
        content.height <= 0)
        return;

    if (state == PageState::Loaded &&
        activePlugin.render) {
        activePlugin.render(content);
        return;
    }

    Image::rectangle(
        content.x, content.y,
        content.width, content.height,
        0x182640
    );

    Image::text(
        content.x + 25,
        content.y + 28,
        "ASTRAL BROWSER",
        WHITE,
        2
    );

    Image::rectangle(
        content.x + 25,
        content.y + 66,
        content.width - 50,
        2,
        PURPLE
    );

    const char* message =
        "Ready for plugin installation.";

    const char* detail =
        "Firefox engine not installed.";

    switch (state) {
        case PageState::Home:
            message = "Welcome to Astral Browser";
            detail = "Online browser engine required.";
            break;

        case PageState::Loading:
            message = "Loading page...";
            detail = "The plugin has the wheel.";
            break;

        case PageState::Offline:
            message = "No internet connection";
            detail = "Check the network driver.";
            break;

        case PageState::PluginMissing:
            message = "Browser plugin unavailable";
            detail = "Firefox runtime not installed.";
            break;

        case PageState::Error:
            message = "Navigation failed";
            detail = "The plugin declined.";
            // The website has rejected us.
            // Just like my last job interview.
            break;

        case PageState::Loaded:
            message = "Page loaded";
            detail = "Renderer unavailable.";
            break;
    }

    Image::text(
        content.x + 25,
        content.y + 95,
        message,
        WHITE
    );

    Image::text(
        content.x + 25,
        content.y + 120,
        detail,
        MUTED
    );

    Image::text(
        content.x + 25,
        content.y + 164,
        "Firefox Plugin",
        PURPLE
    );

    Image::text(
        content.x + 25,
        content.y + 188,
        "Status: not installed",
        MUTED
    );

    Image::text(
        content.x + 25,
        content.y + 218,
        "Requires HTTP, TLS, DNS and engine.",
        MUTED
    );
}

} // namespace Astral::Desktop::Apps::BrowserApp