
#pragma once

#include <stdint.h>

namespace Astral::PCI {

constexpr uint32_t MAX_DEVICES = 128;

enum class BarType : uint8_t {
    Invalid,
    IO,
    Memory32,
    Memory64
};

struct Bar {
    BarType type;
    uint64_t address;
    bool prefetchable;
};

struct Device {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;

    uint16_t vendorId;
    uint16_t deviceId;

    uint8_t classCode;
    uint8_t subclass;
    uint8_t progIF;
    uint8_t revision;

    uint8_t headerType;
    uint8_t interruptLine;
    uint8_t interruptPin;
};

bool initialize();
bool initialized();

uint32_t device_count();

bool device_at(
    uint32_t index,
    Device& device
);

bool find_class(
    uint8_t classCode,
    uint8_t subclass,
    Device& device
);

bool find_device(
    uint16_t vendorId,
    uint16_t deviceId,
    Device& device
);

uint32_t read_config(
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint8_t offset
);

void write_config(
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint8_t offset,
    uint32_t value
);

// Read the configured address of a BAR.
// Does not probe BAR size or map physical memory.
bool read_bar(
    const Device& device,
    uint8_t barIndex,
    Bar& bar
);

}
