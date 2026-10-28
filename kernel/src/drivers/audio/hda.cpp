
#include "hda.h"

#include "../../hardware/pci.h"
#include "../../hardware/mmio.h"

#include <stdint.h>

namespace Astral::HDA {

// ---------------------------------------------
// HD Audio controller registers
// ---------------------------------------------

static constexpr uint32_t REG_GCAP = 0x00;
static constexpr uint32_t REG_GCTL = 0x08;
static constexpr uint32_t REG_WAKE = 0x0C;

static constexpr uint32_t GCTL_CRST = 1u << 0;

static constexpr uint16_t GCAP_64OK = 1u << 0;

static constexpr uint32_t REGISTER_SIZE = 0x1000;

// ---------------------------------------------
// Driver state
// ---------------------------------------------

static PCI::Device controller{};

static volatile void* registers = nullptr;

static Status current{};
static bool ready = false;

// ---------------------------------------------
// Register access
// ---------------------------------------------

static uint32_t read32(uint32_t offset) {
    if (!registers)
        return 0xFFFFFFFFu;

    return MMIO::read32(registers, offset);
}

// ---------------------------------------------
// PCI discovery
// ---------------------------------------------

static bool find_controller(
    PCI::Device& output
) {
    for (uint32_t i = 0;
         i < PCI::device_count();
         ++i) {

        PCI::Device device{};

        if (!PCI::device_at(i, device))
            continue;

        // PCI multimedia / HD Audio controller.
        if (device.classCode == 0x04 &&
            device.subclass == 0x03) {

            output = device;
            return true;
        }
    }

    return false;
}

// ---------------------------------------------
// Controller capability discovery
// ---------------------------------------------

static bool inspect_controller() {
    if (!registers)
        return false;

    // GCAP is bits 0..15.
    // VMIN occupies bits 16..23.
    // VMAJ occupies bits 24..31.
    const uint32_t capabilityRegister =
        read32(REG_GCAP);

    if (capabilityRegister == 0xFFFFFFFFu)
        return false;

    current.globalCapabilities =
        static_cast<uint16_t>(
            capabilityRegister & 0xFFFFu
        );

    current.minorVersion =
        static_cast<uint8_t>(
            capabilityRegister >> 16
        );

    current.majorVersion =
        static_cast<uint8_t>(
            capabilityRegister >> 24
        );

    const uint16_t gcap =
        current.globalCapabilities;

    current.outputStreams =
        static_cast<uint8_t>(
            (gcap >> 12) & 0x0Fu
        );

    current.inputStreams =
        static_cast<uint8_t>(
            (gcap >> 8) & 0x0Fu
        );

    current.bidirectionalStreams =
        static_cast<uint8_t>(
            (gcap >> 3) & 0x1Fu
        );

    current.supports64BitDma =
        (gcap & GCAP_64OK) != 0;

    const uint32_t control =
        read32(REG_GCTL);

    if (control == 0xFFFFFFFFu)
        return false;

    // CRST=0 means controller held in reset.
    current.controllerInReset =
        (control & GCTL_CRST) == 0;

    // Offset 0x0C contains WAKEEN in its low
    // 16 bits and STATESTS in its high 16 bits.
    const uint32_t wake =
        read32(REG_WAKE);

    current.codecStateBits =
        static_cast<uint16_t>(wake >> 16);

    return true;
}

// ---------------------------------------------
// Initialization
// ---------------------------------------------

bool initialize() {
    if (ready)
        return true;

    current = {};
    registers = nullptr;

    if (!PCI::initialized() ||
        !MMIO::available())
        return false;

    if (!find_controller(controller))
        return false;

    current.detected = true;
    current.vendorId = controller.vendorId;
    current.deviceId = controller.deviceId;

    PCI::Bar bar{};

    if (!PCI::read_bar(controller, 0, bar))
        return false;

    if (bar.type != PCI::BarType::Memory32 &&
        bar.type != PCI::BarType::Memory64)
        return false;

    if (!bar.address)
        return false;

    current.physicalBar = bar.address;

    // Enable PCI memory-space access.
    // Bus mastering is NOT enabled here.
    const uint32_t oldCommand =
        PCI::read_config(
            controller.bus,
            controller.slot,
            controller.function,
            0x04
        );

    PCI::write_config(
        controller.bus,
        controller.slot,
        controller.function,
        0x04,
        oldCommand | 0x2u
    );

    registers = MMIO::map(
        bar.address,
        REGISTER_SIZE
    );

    if (!registers)
        return false;

    current.mapped = true;

    if (!inspect_controller())
        return false;

    // No codec commands, streaming or DMA yet.
    current.codecInitialized = false;
    current.streamConfigured = false;
    current.playbackActive = false;

    ready = true;
    return true;
}

bool initialized() {
    return ready;
}

const Status& status() {
    return current;
}

bool refresh() {
    if (!ready || !registers)
        return false;

    return inspect_controller();
}

bool playback_ready() {
    return ready &&
           current.codecInitialized &&
           current.streamConfigured &&
           !current.controllerInReset;
}

}