
#pragma once

#include <stdint.h>

namespace Astral::USB {

enum class ControllerType : uint8_t {
    None,
    UHCI,
    OHCI,
    EHCI,
    XHCI,
    Unknown
};

enum class DeviceSpeed : uint8_t {
    Unknown = 0,
    Full = 1,
    Low = 2,
    High = 3,
    Super = 4,
    SuperPlus = 5
};

struct Port {
    uint8_t number;
    bool connected;
    bool enabled;
    bool powered;
    bool overCurrent;
    DeviceSpeed speed;
};

struct Status {
    bool detected;
    bool registersMapped;

    ControllerType controller;

    uint16_t vendorId;
    uint16_t deviceId;

    uint64_t physicalBar;

    uint16_t interfaceVersion;
    uint8_t capabilityLength;

    uint8_t maximumDeviceSlots;
    uint8_t maximumPorts;

    uint32_t pageSizeMask;
    uint32_t connectedPorts;

    bool controllerHalted;
    bool controllerNotReady;
};

// Discover and inspect USB host-controller hardware.
bool initialize();
bool initialized();

const Status& status();

// Refresh root-port status registers.
// Does not reset ports or enumerate devices.
bool poll();

uint32_t port_count();

bool port_at(
    uint32_t index,
    Port& output
);

// ---------------------------------------------
// USB HID boot-protocol report decoders
// ---------------------------------------------

using KeyboardCallback = void (*)(
    uint8_t modifiers,
    uint8_t keyCode,
    bool pressed
);

using MouseCallback = void (*)(
    int32_t deltaX,
    int32_t deltaY,
    int32_t wheel,
    uint8_t buttons
);

void set_keyboard_callback(
    KeyboardCallback callback
);

void set_mouse_callback(
    MouseCallback callback
);

// Consume an 8-byte boot keyboard input report.
bool process_keyboard_report(
    const uint8_t* report,
    uint32_t length
);

// Consume a 3-byte boot mouse report,
// with an optional fourth wheel byte.
bool process_mouse_report(
    const uint8_t* report,
    uint32_t length
);

// Reset cached HID device state.
void reset_hid_state();

// True only after actual controller/device
// enumeration and input endpoint setup.
// This phase does not implement those operations.
bool hid_ready();

}
