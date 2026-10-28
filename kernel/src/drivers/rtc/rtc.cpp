
#include "rtc.h"

#include <stdint.h>

namespace Astral::RTC {

static constexpr uint16_t CMOS_ADDRESS = 0x70;
static constexpr uint16_t CMOS_DATA = 0x71;

static uint8_t in8(uint16_t port) {
    uint8_t value;

    asm volatile(
        "inb %w1, %0"
        : "=a"(value)
        : "Nd"(port)
        : "memory"
    );

    return value;
}

static void out8(uint16_t port, uint8_t value) {
    asm volatile(
        "outb %0, %w1"
        :
        : "a"(value), "Nd"(port)
        : "memory"
    );
}

static uint8_t cmos_read(uint8_t reg) {
    // Preserve the existing NMI mask bit.
    // CMOS index-port reads are chipset dependent,
    // so NMI control will be centralized with ACPI.
    out8(CMOS_ADDRESS, reg & 0x7F);

    return in8(CMOS_DATA);
}

static bool updating() {
    return (cmos_read(0x0A) & 0x80) != 0;
}

static uint8_t bcd_to_binary(uint8_t value) {
    return static_cast<uint8_t>(
        (value & 0x0F) +
        ((value >> 4) * 10)
    );
}

struct RawTime {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
};

static RawTime read_raw() {
    RawTime t{};

    t.second = cmos_read(0x00);
    t.minute = cmos_read(0x02);
    t.hour = cmos_read(0x04);

    t.day = cmos_read(0x07);
    t.month = cmos_read(0x08);
    t.year = cmos_read(0x09);

    return t;
}

static bool same(
    const RawTime& a,
    const RawTime& b
) {
    return a.second == b.second &&
           a.minute == b.minute &&
           a.hour == b.hour &&
           a.day == b.day &&
           a.month == b.month &&
           a.year == b.year;
}

static bool wait_until_stable() {
    // Avoid unbounded hangs when RTC hardware
    // is unavailable or behaves unexpectedly.
    for (uint32_t attempt = 0;
         attempt < 100000;
         ++attempt) {
        if (!updating())
            return true;

        asm volatile("pause");
    }

    return false;
}

static bool leap_year(uint16_t year) {
    return ((year % 4 == 0) &&
            (year % 100 != 0)) ||
           (year % 400 == 0);
}

bool valid(const DateTime& t) {
    if (t.year < 2000 || t.year > 2099)
        return false;

    if (t.month < 1 || t.month > 12)
        return false;

    if (t.hour > 23 ||
        t.minute > 59 ||
        t.second > 59)
        return false;

    static constexpr uint8_t days[12] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    uint8_t maxDay = days[t.month - 1];

    if (t.month == 2 && leap_year(t.year))
        maxDay = 29;

    return t.day >= 1 && t.day <= maxDay;
}

bool read(DateTime& result) {
    RawTime a{};
    RawTime b{};

    bool stable = false;

    // Read twice around an RTC update.
    for (uint32_t attempt = 0;
         attempt < 8;
         ++attempt) {
        if (!wait_until_stable())
            return false;

        a = read_raw();

        if (!wait_until_stable())
            return false;

        b = read_raw();

        if (same(a, b)) {
            stable = true;
            break;
        }
    }

    if (!stable)
        return false;

    uint8_t statusB = cmos_read(0x0B);

    const bool binaryMode =
        (statusB & 0x04) != 0;

    const bool hour24 =
        (statusB & 0x02) != 0;

    const bool isPM = (a.hour & 0x80) != 0;

    a.hour &= 0x7F;

    if (!binaryMode) {
        a.second = bcd_to_binary(a.second);
        a.minute = bcd_to_binary(a.minute);
        a.hour = bcd_to_binary(a.hour);
        a.day = bcd_to_binary(a.day);
        a.month = bcd_to_binary(a.month);
        a.year = bcd_to_binary(a.year);
    }

    if (!hour24) {
        a.hour %= 12;

        if (isPM)
            a.hour += 12;
    }

    DateTime decoded{};

    decoded.year =
        static_cast<uint16_t>(2000 + a.year);

    decoded.month = a.month;
    decoded.day = a.day;

    decoded.hour = a.hour;
    decoded.minute = a.minute;
    decoded.second = a.second;

    if (!valid(decoded))
        return false;

    result = decoded;
    return true;
}

}
