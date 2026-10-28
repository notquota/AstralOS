
#include "usb_hid.h"

#include "../../hardware/pci.h"
#include "../../hardware/mmio.h"

#include <stdint.h>

namespace Astral::USB {

// ==================================================
// Controller constants
// ==================================================

static constexpr uint8_t PCI_CLASS_SERIAL = 0x0C;
static constexpr uint8_t PCI_SUBCLASS_USB = 0x03;

static constexpr uint32_t XHCI_CAPLENGTH = 0x00;
static constexpr uint32_t XHCI_HCSPARAMS1 = 0x04;
static constexpr uint32_t XHCI_DBOFF = 0x14;
static constexpr uint32_t XHCI_RTSOFF = 0x18;

static constexpr uint32_t XHCI_USBCMD = 0x00;
static constexpr uint32_t XHCI_USBSTS = 0x04;
static constexpr uint32_t XHCI_PAGESIZE = 0x08;

static constexpr uint32_t XHCI_PORT_BASE = 0x400;
static constexpr uint32_t XHCI_PORT_STRIDE = 0x10;

static constexpr uint32_t USBSTS_HCH = 1u << 0;
static constexpr uint32_t USBSTS_CNR = 1u << 11;

static constexpr uint32_t PORTSC_CCS = 1u << 0;
static constexpr uint32_t PORTSC_PED = 1u << 1;
static constexpr uint32_t PORTSC_OCA = 1u << 3;
static constexpr uint32_t PORTSC_PP = 1u << 9;

static constexpr uint32_t PORTSC_SPEED_SHIFT = 10;

static constexpr uint32_t MAX_PORTS = 255;
static constexpr uint32_t MAX_REPORT_KEYS = 6;

// ==================================================
// Driver state
// ==================================================

static PCI::Device pciController{};

static volatile void* registers = nullptr;

static Status current{};
static Port ports[MAX_PORTS]{};

static uint32_t operationalOffset = 0;
static uint32_t mappedLength = 0;

static bool ready = false;

// HID state.
static KeyboardCallback keyboardCallback = nullptr;
static MouseCallback mouseCallback = nullptr;

static uint8_t previousKeys[MAX_REPORT_KEYS]{};
static uint8_t previousModifiers = 0;
static uint8_t previousMouseButtons = 0;

// ==================================================
// Helpers
// ==================================================

static ControllerType classify(uint8_t progIF) {
    switch (progIF) {
        case 0x00:
            return ControllerType::UHCI;

        case 0x10:
            return ControllerType::OHCI;

        case 0x20:
            return ControllerType::EHCI;

        case 0x30:
            return ControllerType::XHCI;

        default:
            return ControllerType::Unknown;
    }
}

static DeviceSpeed decode_speed(uint32_t portsc) {
    const uint8_t speed =
        static_cast<uint8_t>(
            (portsc >> PORTSC_SPEED_SHIFT) & 0x0F
        );

    switch (speed) {
        case 1:
            return DeviceSpeed::Full;

        case 2:
            return DeviceSpeed::Low;

        case 3:
            return DeviceSpeed::High;

        case 4:
            return DeviceSpeed::Super;

        case 5:
            return DeviceSpeed::SuperPlus;

        default:
            return DeviceSpeed::Unknown;
    }
}

static uint32_t read(uint32_t offset) {
    if (!registers ||
        (offset & 3u) != 0 ||
        offset > mappedLength ||
        mappedLength - offset < 4)
        return 0xFFFFFFFFu;

    return MMIO::read32(registers, offset);
}

static bool find_controller(PCI::Device& output) {
    bool found = false;
    PCI::Device fallback{};

    for (uint32_t i = 0;
         i < PCI::device_count();
         ++i) {

        PCI::Device device{};

        if (!PCI::device_at(i, device))
            continue;

        if (device.classCode != PCI_CLASS_SERIAL ||
            device.subclass != PCI_SUBCLASS_USB)
            continue;

        // Prefer an xHCI controller.
        if (device.progIF == 0x30) {
            output = device;
            return true;
        }

        if (!found) {
            fallback = device;
            found = true;
        }
    }

    if (found)
        output = fallback;

    return found;
}

static bool has_key(
    const uint8_t* keys,
    uint8_t code
) {
    if (code == 0)
        return false;

    for (uint32_t i = 0;
         i < MAX_REPORT_KEYS;
         ++i) {

        if (keys[i] == code)
            return true;
    }

    return false;
}

// ==================================================
// Controller discovery
// ==================================================

bool initialize() {
    current = {};
    ready = false;

    registers = nullptr;
    operationalOffset = 0;
    mappedLength = 0;

    reset_hid_state();

    for (uint32_t i = 0; i < MAX_PORTS; ++i)
        ports[i] = {};

    if (!PCI::initialized())
        return false;

    if (!find_controller(pciController))
        return false;

    current.detected = true;
    current.vendorId = pciController.vendorId;
    current.deviceId = pciController.deviceId;

    current.controller =
        classify(pciController.progIF);

    // Non-xHCI controllers require their own
    // register layouts and host drivers.
    if (current.controller != ControllerType::XHCI) {
        ready = true;
        return true;
    }

    if (!MMIO::available())
        return false;

    PCI::Bar bar{};

    if (!PCI::read_bar(pciController, 0, bar))
        return false;

    if (bar.type != PCI::BarType::Memory32 &&
        bar.type != PCI::BarType::Memory64)
        return false;

    if (bar.address == 0)
        return false;

    current.physicalBar = bar.address;

    // Enable PCI memory-space decoding,
    // but do not enable bus mastering yet.
    uint32_t pciCommand =
        PCI::read_config(
            pciController.bus,
            pciController.slot,
            pciController.function,
            0x04
        );

    PCI::write_config(
        pciController.bus,
        pciController.slot,
        pciController.function,
        0x04,
        pciCommand | 0x02u
    );

    // First map the capability register block.
    volatile void* capabilityMapping =
        MMIO::map(bar.address, 0x100);

    if (!capabilityMapping)
        return false;

    uint32_t capBase =
        MMIO::read32(
            capabilityMapping,
            XHCI_CAPLENGTH
        );

    uint32_t hcsParams1 =
        MMIO::read32(
            capabilityMapping,
            XHCI_HCSPARAMS1
        );

    current.capabilityLength =
        static_cast<uint8_t>(capBase);

    current.interfaceVersion =
        static_cast<uint16_t>(capBase >> 16);

    current.maximumDeviceSlots =
        static_cast<uint8_t>(hcsParams1);

    current.maximumPorts =
        static_cast<uint8_t>(
            hcsParams1 >> 24
        );

    // Validate the discovered layout before
    // attempting to map operational registers.
    if (current.capabilityLength < 0x20 ||
        current.maximumPorts == 0)
        return false;

    operationalOffset =
        current.capabilityLength;

    const uint32_t portEnd =
        operationalOffset +
        XHCI_PORT_BASE +
        static_cast<uint32_t>(
            current.maximumPorts
        ) * XHCI_PORT_STRIDE;

    if (portEnd > 0x10000)
        return false;

    // Read these for future command-ring and
    // runtime-register integration.
    const uint32_t doorbellOffset =
        MMIO::read32(
            capabilityMapping,
            XHCI_DBOFF
        ) & ~3u;

    const uint32_t runtimeOffset =
        MMIO::read32(
            capabilityMapping,
            XHCI_RTSOFF
        ) & ~0x1Fu;

    if (doorbellOffset < 0x20 ||
        runtimeOffset < 0x20)
        return false;

    mappedLength = portEnd;

    // Map the complete required capability,
    // operational and root-port register range.
    registers = MMIO::map(
        bar.address,
        mappedLength
    );

    if (!registers)
        return false;

    current.registersMapped = true;

    current.pageSizeMask = read(
        operationalOffset + XHCI_PAGESIZE
    );

    const uint32_t usbStatus = read(
        operationalOffset + XHCI_USBSTS
    );

    current.controllerHalted =
        (usbStatus & USBSTS_HCH) != 0;

    current.controllerNotReady =
        (usbStatus & USBSTS_CNR) != 0;

    ready = true;

    poll();

    return true;
}

bool initialized() {
    return ready;
}

const Status& status() {
    return current;
}

// ==================================================
// Root-port status polling
// ==================================================

bool poll() {
    if (!ready ||
        !registers ||
        current.controller != ControllerType::XHCI)
        return false;

    const uint32_t usbStatus = read(
        operationalOffset + XHCI_USBSTS
    );

    current.controllerHalted =
        (usbStatus & USBSTS_HCH) != 0;

    current.controllerNotReady =
        (usbStatus & USBSTS_CNR) != 0;

    current.connectedPorts = 0;

    for (uint32_t i = 0;
         i < current.maximumPorts;
         ++i) {

        const uint32_t offset =
            operationalOffset +
            XHCI_PORT_BASE +
            i * XHCI_PORT_STRIDE;

        const uint32_t portsc = read(offset);

        Port& port = ports[i];

        port.number =
            static_cast<uint8_t>(i + 1);

        port.connected =
            (portsc & PORTSC_CCS) != 0;

        port.enabled =
            (portsc & PORTSC_PED) != 0;

        port.powered =
            (portsc & PORTSC_PP) != 0;

        port.overCurrent =
            (portsc & PORTSC_OCA) != 0;

        port.speed = decode_speed(portsc);

        if (port.connected)
            ++current.connectedPorts;
    }

    return true;
}

uint32_t port_count() {
    if (!ready)
        return 0;

    return current.maximumPorts;
}

bool port_at(
    uint32_t index,
    Port& output
) {
    if (!ready ||
        index >= current.maximumPorts)
        return false;

    output = ports[index];
    return true;
}

// ==================================================
// HID callback registration
// ==================================================

void set_keyboard_callback(
    KeyboardCallback callback
) {
    keyboardCallback = callback;
}

void set_mouse_callback(
    MouseCallback callback
) {
    mouseCallback = callback;
}

void reset_hid_state() {
    for (uint32_t i = 0;
         i < MAX_REPORT_KEYS;
         ++i) {
        previousKeys[i] = 0;
    }

    previousModifiers = 0;
    previousMouseButtons = 0;
}

// ==================================================
// Boot-protocol keyboard report handling
// ==================================================

bool process_keyboard_report(
    const uint8_t* report,
    uint32_t length
) {
    if (!report || length < 8)
        return false;

    // Standard USB HID boot keyboard report:
    //
    // Byte 0: modifier bits
    // Byte 1: reserved
    // Bytes 2-7: six key usages

    const uint8_t modifiers = report[0];
    const uint8_t* keys = report + 2;

    // Rollover and keyboard error usages.
    // Ignore these reports without synthesizing
    // incorrect key releases.
    for (uint32_t i = 0;
         i < MAX_REPORT_KEYS;
         ++i) {

        if (keys[i] >= 1 && keys[i] <= 3)
            return false;
    }

    if (keyboardCallback) {
        // Releases for keys absent in new report.
        for (uint32_t i = 0;
             i < MAX_REPORT_KEYS;
             ++i) {

            const uint8_t key =
                previousKeys[i];

            if (key != 0 &&
                !has_key(keys, key)) {

                keyboardCallback(
                    modifiers,
                    key,
                    false
                );
            }
        }

        // Presses for newly present keys.
        for (uint32_t i = 0;
             i < MAX_REPORT_KEYS;
             ++i) {

            const uint8_t key = keys[i];

            if (key != 0 &&
                !has_key(previousKeys, key)) {

                keyboardCallback(
                    modifiers,
                    key,
                    true
                );
            }
        }

        // Modifier-only state changes are
        // exposed with keyCode = 0.
        if (previousModifiers != modifiers) {
            keyboardCallback(
                modifiers,
                0,
                true
            );
        }
    }

    for (uint32_t i = 0;
         i < MAX_REPORT_KEYS;
         ++i) {
        previousKeys[i] = keys[i];
    }

    previousModifiers = modifiers;

    return true;
}

// ==================================================
// Boot-protocol mouse report handling
// ==================================================

bool process_mouse_report(
    const uint8_t* report,
    uint32_t length
) {
    if (!report || length < 3)
        return false;

    // Standard HID boot mouse report:
    //
    // Byte 0: button bits
    // Byte 1: signed X movement
    // Byte 2: signed Y movement
    //
    // Some devices provide an additional
    // signed scroll-wheel byte.

    const uint8_t buttons = report[0];

    const int32_t dx =
        static_cast<int8_t>(report[1]);

    const int32_t dy =
        static_cast<int8_t>(report[2]);

    int32_t wheel = 0;

    if (length >= 4) {
        wheel =
            static_cast<int8_t>(report[3]);
    }

    if (mouseCallback) {
        mouseCallback(
            dx,
            dy,
            wheel,
            buttons
        );
    }

    previousMouseButtons = buttons;

    return true;
}

bool hid_ready() {
    // Real USB HID input requires:
    // 1. Controller command/event rings
    // 2. Connected-device enumeration
    // 3. USB device addressing
    // 4. Configuration/interface selection
    // 5. Interrupt IN endpoint transfers
    //
    // These are not yet implemented.
    return false;
}

}
