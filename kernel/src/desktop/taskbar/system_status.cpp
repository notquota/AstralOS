
#include "system_status.h"

#include "../../drivers/rtc/rtc.h"
#include "../../config/settings.h"

namespace Astral::Desktop::SystemStatus {

static BatteryReader batteryReader = nullptr;

static Battery currentBattery{};
static Clock currentClock{};

static char timeBuffer[16] = "--:--";
static char batteryBuffer[16] = "N/A";

static void two_digits(
    char* out,
    uint32_t value
) {
    out[0] = static_cast<char>(
        '0' + (value / 10) % 10
    );
    out[1] = static_cast<char>(
        '0' + value % 10
    );
}

static void format_time() {
    if (!currentClock.available) {
        timeBuffer[0] = '-';
        timeBuffer[1] = '-';
        timeBuffer[2] = ':';
        timeBuffer[3] = '-';
        timeBuffer[4] = '-';
        timeBuffer[5] = '\0';
        return;
    }

    uint32_t hour = currentClock.hour;

    if (Config::get().use24HourClock) {
        two_digits(timeBuffer, hour);
        timeBuffer[2] = ':';
        two_digits(timeBuffer + 3, currentClock.minute);
        timeBuffer[5] = '\0';
    } else {
        const bool pm = hour >= 12;

        hour %= 12;
        if (hour == 0)
            hour = 12;

        two_digits(timeBuffer, hour);
        timeBuffer[2] = ':';
        two_digits(timeBuffer + 3, currentClock.minute);

        timeBuffer[5] = ' ';
        timeBuffer[6] = pm ? 'P' : 'A';
        timeBuffer[7] = 'M';
        timeBuffer[8] = '\0';
    }
}

static void format_battery() {
    if (!currentBattery.available) {
        batteryBuffer[0] = 'N';
        batteryBuffer[1] = '/';
        batteryBuffer[2] = 'A';
        batteryBuffer[3] = '\0';
        return;
    }

    const uint32_t percent = currentBattery.percent;

    uint32_t index = 0;

    if (percent >= 100) {
        batteryBuffer[index++] = '1';
        batteryBuffer[index++] = '0';
        batteryBuffer[index++] = '0';
    } else if (percent >= 10) {
        batteryBuffer[index++] =
            static_cast<char>('0' + percent / 10);

        batteryBuffer[index++] =
            static_cast<char>('0' + percent % 10);
    } else {
        batteryBuffer[index++] =
            static_cast<char>('0' + percent);
    }

    batteryBuffer[index++] = '%';
    batteryBuffer[index] = '\0';
}

void initialize() {
    currentBattery = {};
    currentClock = {};

    format_time();
    format_battery();
}

void set_battery_reader(BatteryReader reader) {
    batteryReader = reader;
}

void refresh() {
    RTC::DateTime rtc{};

    currentClock.available = RTC::read(rtc);

    if (currentClock.available) {
        currentClock.year = rtc.year;
        currentClock.month = rtc.month;
        currentClock.day = rtc.day;
        currentClock.hour = rtc.hour;
        currentClock.minute = rtc.minute;
        currentClock.second = rtc.second;
    }

    currentBattery = {};

    if (batteryReader) {
        Battery result{};

        if (batteryReader(result) &&
            result.available &&
            result.percent <= 100) {
            currentBattery = result;
        }
    }

    format_time();
    format_battery();
}

Battery battery() {
    return currentBattery;
}

Clock clock() {
    return currentClock;
}

const char* time_text() {
    return timeBuffer;
}

const char* battery_text() {
    return batteryBuffer;
}

}
