#include "mouse.h"

#include "../arch/x86_64/lowlevel.h"
#include "../graphics/framebuffer.h"
#include "../graphics/console.h"
#include "../config/settings.h"

#include <stdint.h>

namespace Astral::Mouse {

namespace {

constexpr uint16_t DATA_PORT = 0x60;
constexpr uint16_t STATUS_PORT = 0x64;
constexpr uint16_t COMMAND_PORT = 0x64;

constexpr uint32_t TIMEOUT = 1000000;

bool ready = false;
bool uiCaptured = false;

bool leftHeld = false;
bool clickPending = false;

int32_t pointerX = 0;
int32_t pointerY = 0;
int32_t scrollPending = 0;

uint64_t eventRevision = 0;
uint8_t deviceId = 0;

uint8_t packet[4]{};
uint8_t packetPosition = 0;

bool wait_write() {
    for (uint32_t i = 0; i < TIMEOUT; ++i) {
        if (!(astral_in8(STATUS_PORT) & 0x02))
            return true;

        astral_pause();
    }

    return false;
}

bool read_controller(uint8_t& value) {
    for (uint32_t i = 0; i < TIMEOUT; ++i) {
        if (astral_in8(STATUS_PORT) & 0x01) {
            value = astral_in8(DATA_PORT);
            return true;
        }

        astral_pause();
    }

    return false;
}

bool read_mouse(uint8_t& value) {
    for (uint32_t i = 0; i < TIMEOUT; ++i) {
        const uint8_t status =
            astral_in8(STATUS_PORT);

        if (!(status & 0x01) || !(status & 0x20)) {
            astral_pause();
            continue;
        }

        value = astral_in8(DATA_PORT);
        return true;
    }

    return false;
}

bool send_controller(uint8_t command) {
    if (!wait_write())
        return false;

    astral_out8(COMMAND_PORT, command);
    return true;
}

bool send_mouse(uint8_t command) {
    if (!send_controller(0xD4))
        return false;

    if (!wait_write())
        return false;

    astral_out8(DATA_PORT, command);
    return true;
}

bool mouse_command(uint8_t command) {
    if (!send_mouse(command))
        return false;

    uint8_t response = 0;

    if (!read_mouse(response))
        return false;

    return response == 0xFA;
}

bool set_sample_rate(uint8_t rate) {
    return mouse_command(0xF3) &&
           mouse_command(rate);
}

bool get_device_id(uint8_t& id) {
    if (!mouse_command(0xF2))
        return false;

    return read_mouse(id);
}

void clamp_pointer() {
    int32_t maxX =
        static_cast<int32_t>(Graphics::width()) - 1;

    int32_t maxY =
        static_cast<int32_t>(Graphics::height()) - 1;

    if (maxX < 0) maxX = 0;
    if (maxY < 0) maxY = 0;

    if (pointerX < 0) pointerX = 0;
    if (pointerY < 0) pointerY = 0;

    if (pointerX > maxX) pointerX = maxX;
    if (pointerY > maxY) pointerY = maxY;
}

void process_packet() {
    const uint8_t flags = packet[0];

    // Invalid movement packet.
    // The mouse is attempting financial fraud.
    if (flags & 0xC0)
        return;

    int32_t dx = packet[1];
    int32_t dy = packet[2];

    if (flags & 0x10) dx -= 256;
    if (flags & 0x20) dy -= 256;

    const auto& settings = Config::get();

    const int32_t oldX = pointerX;
    const int32_t oldY = pointerY;

    pointerX +=
        dx * static_cast<int32_t>(
            settings.pointerSpeed
        );

    pointerY -=
        dy * static_cast<int32_t>(
            settings.pointerSpeed
        );

    clamp_pointer();

    const bool leftNow =
        (flags & 0x01) != 0;

    const bool buttonChanged =
        leftNow != leftHeld;

    if (leftNow && !leftHeld)
        clickPending = true;

    leftHeld = leftNow;

    if (pointerX != oldX ||
        pointerY != oldY ||
        buttonChanged) {
        ++eventRevision;
    }

    if (deviceId != 3 && deviceId != 4)
        return;

    int32_t wheel = packet[3] & 0x0F;

    if (wheel & 0x08)
        wheel -= 16;

    if (wheel == 0)
        return;

    const int32_t lines =
        wheel * static_cast<int32_t>(
            settings.scrollLines
        );

    if (uiCaptured) {
        int32_t next = scrollPending + lines;

        if (next > 120) next = 120;
        if (next < -120) next = -120;

        scrollPending = next;
    } else {
        Console::scroll_lines(-lines);
    }

    ++eventRevision;
}

} // namespace

bool initialize() {
    ready = false;
    uiCaptured = false;

    leftHeld = false;
    clickPending = false;
    scrollPending = 0;

    eventRevision = 0;
    deviceId = 0;

    packetPosition = 0;

    pointerX =
        static_cast<int32_t>(Graphics::width() / 2);

    pointerY =
        static_cast<int32_t>(Graphics::height() / 2);

    clamp_pointer();

    if (!send_controller(0xA8))
        return false;

    if (!send_controller(0x20))
        return false;

    uint8_t configuration = 0;

    if (!read_controller(configuration))
        return false;

    configuration &=
        static_cast<uint8_t>(~0x20);

    // This build uses mouse polling.
    // IRQ12 is off, so somebody must actually poll.
    // Previous versions apparently forgot that detail.
    configuration &=
        static_cast<uint8_t>(~0x02);

    if (!send_controller(0x60))
        return false;

    if (!wait_write())
        return false;

    astral_out8(DATA_PORT, configuration);

    if (!mouse_command(0xF5))
        return false;

    if (!mouse_command(0xF6))
        return false;

    const bool wheelNegotiated =
        set_sample_rate(200) &&
        set_sample_rate(100) &&
        set_sample_rate(80);

    uint8_t id = 0;

    if (!get_device_id(id))
        return false;

    if (wheelNegotiated &&
        (id == 3 || id == 4)) {
        deviceId = id;
    }

    if (!mouse_command(0xF4))
        return false;

    ready = true;
    return true;
}

bool initialized() {
    return ready;
}

bool wheel_supported() {
    return ready && (deviceId == 3 || deviceId == 4);
}

int32_t x() {
    return pointerX;
}

int32_t y() {
    return pointerY;
}

uint64_t revision() {
    return eventRevision;
}

bool left_down() {
    return leftHeld;
}

bool take_left_click() {
    const bool value = clickPending;
    clickPending = false;
    return value;
}

int32_t take_scroll() {
    const int32_t result = scrollPending;
    scrollPending = 0;
    return result;
}

void set_ui_capture(bool captured) {
    uiCaptured = captured;

    clickPending = false;
    scrollPending = 0;

    ++eventRevision;
}

void feed(uint8_t data) {
    if (!ready)
        return;

    // Byte zero needs PS/2 synchronization bit 3.
    // Without it we're just reading mouse poetry.
    if (packetPosition == 0 && !(data & 0x08))
        return;

    packet[packetPosition++] = data;

    const uint8_t size =
        (deviceId == 3 || deviceId == 4)
            ? 4
            : 3;

    if (packetPosition < size)
        return;

    packetPosition = 0;
    process_packet();
}

void poll() {
    if (!ready)
        return;

    // Don't steal keyboard bytes.
    // The keyboard has suffered enough.
    //
    // Important: the PS/2 controller is shared.
    // This assumes another input consumer is not
    // simultaneously reading mouse data from 0x60.
    for (uint32_t i = 0; i < 32; ++i) {
        const uint8_t status =
            astral_in8(STATUS_PORT);

        // No pending byte.
        if (!(status & 0x01))
            break;

        // Next byte belongs to keyboard/controller,
        // not the auxiliary mouse device.
        if (!(status & 0x20))
            break;

        const uint8_t value =
            astral_in8(DATA_PORT);

        feed(value);
    }
}

} // namespace Astral::Mouse