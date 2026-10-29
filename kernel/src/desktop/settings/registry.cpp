
#include "registry.h"

namespace Astral::Desktop::Settings {

namespace {

struct Seed {
    const char* name;
    Type type;
    int32_t minValue;
    int32_t maxValue;
    int32_t step;
    int32_t initial;
    const char* choices;
};

#define T(n, v) {n, Type::Toggle, 0, 1, 1, v, nullptr}
#define N(n, lo, hi, step, v) \
    {n, Type::Integer, lo, hi, step, v, nullptr}
#define C(n, maximum, v, labels) \
    {n, Type::Choice, 0, maximum, 1, v, labels}

// Six definitions for each of the 20 categories.
// Values are local settings until a backend subscribes
// to changes and applies them to the OS.

static constexpr Seed seeds[SETTING_COUNT] = {
    // 0: Appearance
    C("Theme", 2, 0, "Dark|Light|System"),
    C("Accent", 5, 0, "Violet|Blue|Green|Amber|Red|Cyan"),
    N("UI Scale", 75, 200, 5, 100),
    T("Animations", 1),
    T("Transparency", 1),
    N("Corner Radius", 0, 24, 2, 10),

    // 1: Desktop
    T("Desktop Icons", 1),
    T("Show Taskbar", 1),
    C("Taskbar Position", 2, 0, "Bottom|Top|Left"),
    N("Icon Size", 24, 96, 4, 48),
    T("Show Wallpaper", 1),
    T("Snap Windows", 1),

    // 2: Displays
    N("Brightness Preference", 0, 100, 5, 80),
    N("Contrast Preference", 0, 100, 5, 50),
    C("Rotation", 3, 0, "0|90|180|270"),
    T("Night Mode", 0),
    N("Night Warmth", 0, 100, 5, 50),
    T("Screen Blanking", 1),

    // 3: Input
    N("Pointer Speed", 1, 20, 1, 10),
    N("Scroll Lines", 1, 12, 1, 3),
    T("Natural Scrolling", 0),
    T("Pointer Visible", 1),
    T("Keyboard Repeat", 1),
    N("Repeat Delay", 150, 1000, 50, 400),

    // 4: Sound
    N("Master Volume", 0, 100, 5, 70),
    T("Mute", 0),
    T("System Sounds", 1),
    N("Notification Volume", 0, 100, 5, 60),
    N("Microphone Level", 0, 100, 5, 70),
    C("Sound Profile", 2, 0, "Balanced|Quiet|Performance"),

    // 5: Network
    T("Network Enabled", 1),
    T("Ethernet Enabled", 1),
    T("Wi-Fi Enabled", 1),
    T("Automatic DHCP", 1),
    T("DNS Cache", 1),
    T("Network Notifications", 1),

    // 6: Bluetooth
    T("Bluetooth Enabled", 0),
    T("Discoverable", 0),
    T("Auto Reconnect", 1),
    T("Show Bluetooth Icon", 1),
    N("Discovery Timeout", 10, 300, 10, 60),
    T("Connection Notifications", 1),

    // 7: Storage
    T("Show Hidden Files", 0),
    T("Confirm Delete", 1),
    T("Show Extensions", 1),
    C("Default View", 2, 0, "Icons|List|Details"),
    N("Thumbnail Size", 32, 128, 8, 64),
    T("Show Storage Devices", 1),

    // 8: Applications
    T("App Notifications", 1),
    T("Remember Window Size", 1),
    T("Restore Last Session", 0),
    T("Confirm App Exit", 0),
    T("Open Files in Tabs", 0),
    N("Recent App Limit", 0, 30, 1, 10),

    // 9: Notifications
    T("Notifications Enabled", 1),
    T("Show Banners", 1),
    T("Play Sounds", 1),
    N("Banner Duration", 2, 15, 1, 5),
    T("Do Not Disturb", 0),
    T("Show Notification Count", 1),

    // 10: Power
    C("Power Profile", 2, 0, "Balanced|Saver|Performance"),
    N("Screen Timeout Min", 1, 120, 1, 10),
    T("Battery Percentage", 1),
    T("Low Battery Warning", 1),
    N("Low Battery Threshold", 5, 30, 5, 15),
    T("Dim Display On Idle", 1),

    // 11: Date & Time
    T("24 Hour Clock", 1),
    T("Show Seconds", 0),
    T("Show Date", 1),
    C("Date Format", 2, 0, "YYYY-MM-DD|DD/MM/YYYY|MM/DD/YYYY"),
    N("UTC Offset Minutes", -720, 840, 30, 0),
    T("Clock In Taskbar", 1),

    // 12: Language
    C("Language", 3, 0, "English|French|German|Spanish"),
    C("Region", 3, 0, "Canada|USA|UK|Other"),
    T("Spell Check", 0),
    T("Auto Capitalization", 0),
    C("Keyboard Layout", 2, 0, "US|UK|Canadian"),
    T("Use Regional Numbers", 1),

    // 13: Accessibility
    T("High Contrast", 0),
    N("Text Scale", 75, 200, 5, 100),
    T("Reduce Motion", 0),
    T("Large Cursor", 0),
    T("Sticky Keys", 0),
    T("Focus Highlight", 1),

    // 14: Accounts
    T("Show Account Picture", 1),
    T("Remember Last User", 1),
    T("Show Username On Lock", 1),
    T("Confirm Account Switch", 1),
    T("Session Notifications", 1),
    N("Session Idle Min", 1, 240, 1, 30),

    // 15: Privacy
    T("Clipboard History", 0),
    T("Usage Statistics", 0),
    T("Crash Reports", 0),
    T("Remember Recent Files", 1),
    T("Location Permission", 0),
    T("Camera Permission", 0),

    // 16: Security
    // Lock credentials and lock enforcement live in
    // security/lock.*; these are UI preferences only.
    T("Show Lock Background", 1),
    T("Show Lock Clock", 1),
    T("Show Lock Date", 1),
    T("Hide Notifications On Lock", 1),
    T("Mask Password Input", 1),
    T("Show Failed Login Warning", 1),

    // 17: Updates
    T("Check Updates Automatically", 0),
    T("Notify About Updates", 1),
    C("Update Channel", 2, 0, "Alpha|Beta|Stable"),
    T("Allow Preview Releases", 1),
    T("Show Release Notes", 1),
    T("Confirm Before Updating", 1),

    // 18: Developer
    T("Developer Mode", 0),
    T("Show Debug Overlay", 0),
    T("Log Desktop Events", 0),
    T("Show Frame Rate", 0),
    T("Show Window Borders", 0),
    N("Log Level", 0, 4, 1, 1),

    // 19: About
    T("Show Build Metadata", 1),
    T("Show Kernel Version", 1),
    T("Show Architecture", 1),
    T("Show Driver Summary", 1),
    T("Show License Information", 1),
    T("Show Hardware Information", 1)
};

#undef T
#undef N
#undef C

static const char* CATEGORY_NAMES[CATEGORY_COUNT] = {
    "Appearance", "Desktop", "Displays", "Input",
    "Sound", "Network", "Bluetooth", "Storage",
    "Applications", "Notifications", "Power & Battery",
    "Date & Time", "Language", "Accessibility",
    "Accounts", "Privacy", "Security", "Updates",
    "Developer", "About"
};

Item items[SETTING_COUNT]{};
ChangeListener listener = nullptr;
bool ready = false;

static uint32_t index_for(
    Category category,
    uint32_t position
) {
    return static_cast<uint32_t>(category) *
           ITEMS_PER_CATEGORY + position;
}

static int32_t validated(
    const Definition& d,
    int32_t value
) {
    if (value < d.minimum) value = d.minimum;
    if (value > d.maximum) value = d.maximum;

    if (d.type == Type::Toggle)
        return value ? 1 : 0;

    const int32_t step = d.step > 0 ? d.step : 1;
    return d.minimum +
        ((value - d.minimum) / step) * step;
}

static void ensure_ready() {
    if (!ready)
        initialize();
}

}

void initialize() {
    listener = nullptr;

    for (uint32_t i = 0; i < SETTING_COUNT; ++i) {
        const Seed& s = seeds[i];

        Definition definition{};
        definition.id = i + 1;
        definition.category = static_cast<Category>(
            i / ITEMS_PER_CATEGORY
        );
        definition.name = s.name;
        definition.type = s.type;
        definition.minimum = s.minValue;
        definition.maximum = s.maxValue;
        definition.step = s.step;
        definition.defaultValue = s.initial;
        definition.choices = s.choices;

        items[i].definition = definition;
        items[i].value = validated(
            definition, definition.defaultValue
        );
        items[i].modified = false;
    }

    ready = true;
}

uint32_t count() {
    return SETTING_COUNT;
}

const Item* at(uint32_t index) {
    ensure_ready();
    return index < SETTING_COUNT ? &items[index] : nullptr;
}

const Item* find(uint32_t id) {
    if (id == 0 || id > SETTING_COUNT)
        return nullptr;

    return at(id - 1);
}

bool get(uint32_t id, int32_t& value) {
    const Item* item = find(id);
    if (!item) return false;

    value = item->value;
    return true;
}

bool set(uint32_t id, int32_t value) {
    if (id == 0 || id > SETTING_COUNT)
        return false;

    ensure_ready();

    Item& item = items[id - 1];
    const int32_t next = validated(item.definition, value);
    const int32_t previous = item.value;

    if (next == previous)
        return true;

    item.value = next;
    item.modified =
        next != item.definition.defaultValue;

    if (listener)
        listener(id, previous, next);

    return true;
}

bool toggle(uint32_t id) {
    const Item* item = find(id);
    if (!item || item->definition.type != Type::Toggle)
        return false;

    return set(id, item->value ? 0 : 1);
}

bool increment(uint32_t id, int32_t direction) {
    const Item* item = find(id);

    if (!item || direction == 0)
        return false;

    if (item->definition.type == Type::Toggle)
        return toggle(id);

    const int32_t step =
        item->definition.step > 0
            ? item->definition.step : 1;

    const int64_t candidate =
        static_cast<int64_t>(item->value) +
        (direction > 0 ? step : -step);

    const int32_t bounded =
        candidate > item->definition.maximum
            ? item->definition.maximum
            : candidate < item->definition.minimum
                ? item->definition.minimum
                : static_cast<int32_t>(candidate);

    return set(id, bounded);
}

bool reset(uint32_t id) {
    const Item* item = find(id);
    return item
        ? set(id, item->definition.defaultValue)
        : false;
}

void reset_category(Category category) {
    const uint32_t categoryIndex =
        static_cast<uint32_t>(category);

    if (categoryIndex >= CATEGORY_COUNT)
        return;

    for (uint32_t i = 0; i < ITEMS_PER_CATEGORY; ++i)
        reset(index_for(category, i) + 1);
}

void reset_all() {
    for (uint32_t i = 1; i <= SETTING_COUNT; ++i)
        reset(i);
}

void set_change_listener(ChangeListener callback) {
    ensure_ready();
    listener = callback;
}

const char* category_name(Category category) {
    const uint32_t index =
        static_cast<uint32_t>(category);

    return index < CATEGORY_COUNT
        ? CATEGORY_NAMES[index]
        : "Unknown";
}

uint32_t category_count(Category category) {
    return static_cast<uint32_t>(category) < CATEGORY_COUNT
        ? ITEMS_PER_CATEGORY : 0;
}

const Item* category_item(
    Category category,
    uint32_t index
) {
    if (index >= category_count(category))
        return nullptr;

    return at(index_for(category, index));
}

const char* choice_label(const Item& item) {
    if (item.definition.type != Type::Choice ||
        !item.definition.choices)
        return "";

    static char buffer[48];
    uint32_t output = 0;
    int32_t currentChoice = 0;

    const char* source = item.definition.choices;

    while (*source && currentChoice < item.value) {
        if (*source == '|')
            ++currentChoice;

        ++source;
    }

    while (*source && *source != '|' &&
           output + 1 < sizeof(buffer)) {
        buffer[output++] = *source++;
    }

    buffer[output] = '\0';
    return buffer;
}

}
