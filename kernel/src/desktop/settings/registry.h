
#pragma once

#include <stdint.h>

namespace Astral::Desktop::Settings {

constexpr uint32_t CATEGORY_COUNT = 20;
constexpr uint32_t ITEMS_PER_CATEGORY = 6;
constexpr uint32_t SETTING_COUNT =
    CATEGORY_COUNT * ITEMS_PER_CATEGORY;

enum class Type : uint8_t {
    Toggle,
    Integer,
    Choice
};

enum class Category : uint8_t {
    Appearance,
    Desktop,
    Displays,
    Input,
    Sound,
    Network,
    Bluetooth,
    Storage,
    Applications,
    Notifications,
    Power,
    DateTime,
    Language,
    Accessibility,
    Accounts,
    Privacy,
    Security,
    Updates,
    Developer,
    About
};

struct Definition {
    uint32_t id;
    Category category;

    const char* name;
    Type type;

    int32_t minimum;
    int32_t maximum;
    int32_t step;
    int32_t defaultValue;

    const char* choices;
};

struct Item {
    Definition definition;
    int32_t value;
    bool modified;
};

using ChangeListener = void (*)(
    uint32_t id,
    int32_t previousValue,
    int32_t newValue
);

void initialize();

uint32_t count();

const Item* at(uint32_t index);
const Item* find(uint32_t id);

bool get(uint32_t id, int32_t& value);
bool set(uint32_t id, int32_t value);

bool toggle(uint32_t id);
bool increment(uint32_t id, int32_t direction);

bool reset(uint32_t id);
void reset_category(Category category);
void reset_all();

void set_change_listener(ChangeListener listener);

const char* category_name(Category category);
const char* choice_label(const Item& item);

uint32_t category_count(Category category);

const Item* category_item(
    Category category,
    uint32_t index
);

}
