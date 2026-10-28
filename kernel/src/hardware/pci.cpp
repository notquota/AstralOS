
#include "pci.h"

namespace Astral::PCI {

static Device devices[MAX_DEVICES]{};

static uint32_t totalDevices = 0;
static bool ready = false;

static constexpr uint16_t CONFIG_ADDRESS = 0xCF8;
static constexpr uint16_t CONFIG_DATA = 0xCFC;

static void out32(uint16_t port, uint32_t value) {
    asm volatile(
        "outl %0, %w1"
        :
        : "a"(value), "Nd"(port)
        : "memory"
    );
}

static uint32_t in32(uint16_t port) {
    uint32_t value = 0;

    asm volatile(
        "inl %w1, %0"
        : "=a"(value)
        : "Nd"(port)
        : "memory"
    );

    return value;
}

static uint32_t config_address(
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint8_t offset
) {
    return 0x80000000u |
        (static_cast<uint32_t>(bus) << 16) |
        (static_cast<uint32_t>(slot & 0x1F) << 11) |
        (static_cast<uint32_t>(function & 7) << 8) |
        (offset & 0xFC);
}

uint32_t read_config(
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint8_t offset
) {
    out32(
        CONFIG_ADDRESS,
        config_address(bus, slot, function, offset)
    );

    return in32(CONFIG_DATA);
}

void write_config(
    uint8_t bus,
    uint8_t slot,
    uint8_t function,
    uint8_t offset,
    uint32_t value
) {
    out32(
        CONFIG_ADDRESS,
        config_address(bus, slot, function, offset)
    );

    out32(CONFIG_DATA, value);
}

static void inspect(
    uint8_t bus,
    uint8_t slot,
    uint8_t function
) {
    const uint32_t identity =
        read_config(bus, slot, function, 0x00);

    const uint16_t vendor =
        static_cast<uint16_t>(identity);

    if (vendor == 0xFFFF)
        return;

    if (totalDevices >= MAX_DEVICES)
        return;

    const uint32_t classInfo =
        read_config(bus, slot, function, 0x08);

    const uint32_t headerInfo =
        read_config(bus, slot, function, 0x0C);

    const uint32_t interrupts =
        read_config(bus, slot, function, 0x3C);

    Device& d = devices[totalDevices++];

    d.bus = bus;
    d.slot = slot;
    d.function = function;

    d.vendorId = vendor;
    d.deviceId =
        static_cast<uint16_t>(identity >> 16);

    d.classCode =
        static_cast<uint8_t>(classInfo >> 24);

    d.subclass =
        static_cast<uint8_t>(classInfo >> 16);

    d.progIF =
        static_cast<uint8_t>(classInfo >> 8);

    d.revision =
        static_cast<uint8_t>(classInfo);

    d.headerType =
        static_cast<uint8_t>(headerInfo >> 16);

    d.interruptLine =
        static_cast<uint8_t>(interrupts);

    d.interruptPin =
        static_cast<uint8_t>(interrupts >> 8);
}

bool initialize() {
    totalDevices = 0;
    ready = false;

    for (uint32_t bus = 0; bus < 256; ++bus) {
        for (uint32_t slot = 0; slot < 32; ++slot) {
            uint8_t b = static_cast<uint8_t>(bus);
            uint8_t s = static_cast<uint8_t>(slot);

            uint32_t id =
                read_config(b, s, 0, 0x00);

            if ((id & 0xFFFF) == 0xFFFF)
                continue;

            uint32_t header =
                read_config(b, s, 0, 0x0C);

            uint8_t headerType =
                static_cast<uint8_t>(header >> 16);

            uint32_t functionCount =
                (headerType & 0x80) ? 8 : 1;

            for (uint32_t function = 0;
                 function < functionCount;
                 ++function) {
                inspect(
                    b,
                    s,
                    static_cast<uint8_t>(function)
                );
            }
        }
    }

    ready = true;
    return true;
}

bool initialized() {
    return ready;
}

uint32_t device_count() {
    return totalDevices;
}

bool device_at(
    uint32_t index,
    Device& output
) {
    if (!ready || index >= totalDevices)
        return false;

    output = devices[index];
    return true;
}

bool find_class(
    uint8_t classCode,
    uint8_t subclass,
    Device& output
) {
    if (!ready)
        return false;

    for (uint32_t i = 0; i < totalDevices; ++i) {
        const Device& d = devices[i];

        if (d.classCode == classCode &&
            d.subclass == subclass) {
            output = d;
            return true;
        }
    }

    return false;
}

bool find_device(
    uint16_t vendorId,
    uint16_t deviceId,
    Device& output
) {
    if (!ready)
        return false;

    for (uint32_t i = 0; i < totalDevices; ++i) {
        const Device& d = devices[i];

        if (d.vendorId == vendorId &&
            d.deviceId == deviceId) {
            output = d;
            return true;
        }
    }

    return false;
}

bool read_bar(
    const Device& device,
    uint8_t barIndex,
    Bar& bar
) {
    bar = {BarType::Invalid, 0, false};

    // Type 0 PCI headers have six BARs.
    // Type 1 PCI bridge headers have two.
    const uint8_t headerType =
        device.headerType & 0x7F;

    uint8_t maxBars = 0;

    if (headerType == 0)
        maxBars = 6;
    else if (headerType == 1)
        maxBars = 2;
    else
        return false;

    if (barIndex >= maxBars)
        return false;

    const uint8_t offset =
        static_cast<uint8_t>(0x10 + barIndex * 4);

    const uint32_t low = read_config(
        device.bus,
        device.slot,
        device.function,
        offset
    );

    if (low == 0)
        return false;

    if (low & 1) {
        bar.type = BarType::IO;
        bar.address =
            static_cast<uint64_t>(low & ~0x3u);
        return true;
    }

    bar.prefetchable = (low & 0x8) != 0;

    const uint32_t memoryType =
        (low >> 1) & 0x3;

    if (memoryType == 0) {
        bar.type = BarType::Memory32;
        bar.address =
            static_cast<uint64_t>(low & ~0xFu);
        return true;
    }

    if (memoryType == 2 &&
        barIndex + 1 < maxBars) {
        const uint32_t high = read_config(
            device.bus,
            device.slot,
            device.function,
            static_cast<uint8_t>(offset + 4)
        );

        bar.type = BarType::Memory64;

        bar.address =
            (static_cast<uint64_t>(high) << 32) |
            static_cast<uint64_t>(low & ~0xFu);

        return true;
    }

    return false;
}

}
