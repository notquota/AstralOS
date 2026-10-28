
#include "e1000.h"

#include "../../hardware/pci.h"
#include "../../hardware/mmio.h"
#include "../../memory/dma.h"

#include <stdint.h>

namespace Astral::E1000 {

// ================================================
// Hardware definitions
// ================================================

static constexpr uint16_t INTEL = 0x8086;
static constexpr uint16_t I82540EM = 0x100E;
static constexpr uint16_t I82545EM = 0x100F;

static constexpr uint32_t REG_CTRL   = 0x0000;
static constexpr uint32_t REG_STATUS = 0x0008;
static constexpr uint32_t REG_ICR    = 0x00C0;
static constexpr uint32_t REG_IMC    = 0x00D8;

static constexpr uint32_t REG_RCTL   = 0x0100;
static constexpr uint32_t REG_TCTL   = 0x0400;
static constexpr uint32_t REG_TIPG   = 0x0410;

static constexpr uint32_t REG_RDBAL  = 0x2800;
static constexpr uint32_t REG_RDBAH  = 0x2804;
static constexpr uint32_t REG_RDLEN  = 0x2808;
static constexpr uint32_t REG_RDH    = 0x2810;
static constexpr uint32_t REG_RDT    = 0x2818;

static constexpr uint32_t REG_TDBAL  = 0x3800;
static constexpr uint32_t REG_TDBAH  = 0x3804;
static constexpr uint32_t REG_TDLEN  = 0x3808;
static constexpr uint32_t REG_TDH    = 0x3810;
static constexpr uint32_t REG_TDT    = 0x3818;

static constexpr uint32_t REG_RAL0   = 0x5400;
static constexpr uint32_t REG_RAH0   = 0x5404;

static constexpr uint32_t CTRL_RST  = 1u << 26;
static constexpr uint32_t CTRL_SLU  = 1u << 6;
static constexpr uint32_t CTRL_ASDE = 1u << 5;

static constexpr uint32_t STATUS_LU = 1u << 1;

static constexpr uint32_t RCTL_EN    = 1u << 1;
static constexpr uint32_t RCTL_BAM   = 1u << 15;
static constexpr uint32_t RCTL_SECRC = 1u << 26;

static constexpr uint32_t TCTL_EN  = 1u << 1;
static constexpr uint32_t TCTL_PSP = 1u << 3;

static constexpr uint8_t RX_DD  = 1u << 0;
static constexpr uint8_t RX_EOP = 1u << 1;

static constexpr uint8_t TX_EOP = 1u << 0;
static constexpr uint8_t TX_RS  = 1u << 3;
static constexpr uint8_t TX_DD  = 1u << 0;

static constexpr uint32_t RX_COUNT = 32;
static constexpr uint32_t TX_COUNT = 16;

static constexpr uint32_t BUFFER_SIZE = 2048;
static constexpr uint32_t MAX_FRAME = 1514;

static constexpr uint32_t RESET_TIMEOUT = 2000000;
static constexpr uint32_t TX_TIMEOUT = 2000000;

// ================================================
// Legacy E1000 descriptor formats
// ================================================

struct RxDescriptor {
    uint64_t address;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed));

struct TxDescriptor {
    uint64_t address;
    uint16_t length;
    uint8_t checksumOffset;
    uint8_t command;
    uint8_t status;
    uint8_t checksumStart;
    uint16_t special;
} __attribute__((packed));

static_assert(sizeof(RxDescriptor) == 16);
static_assert(sizeof(TxDescriptor) == 16);

// ================================================
// Driver state
// ================================================

static PCI::Device pciDevice{};
static volatile void* registers = nullptr;

static bool detected = false;
static bool active = false;

static MacAddress currentMac{};
static bool macValid = false;

static DMA::Buffer rxRingMemory{};
static DMA::Buffer txRingMemory{};

static DMA::Buffer rxBuffers[RX_COUNT]{};
static DMA::Buffer txBuffers[TX_COUNT]{};

static volatile RxDescriptor* rxRing = nullptr;
static volatile TxDescriptor* txRing = nullptr;

static uint32_t rxIndex = 0;
static uint32_t txIndex = 0;

static ReceiveCallback receiveCallback = nullptr;
static Statistics counters{};

// ================================================
// Hardware access
// ================================================

static uint32_t read(uint32_t offset) {
    return MMIO::read32(registers, offset);
}

static void write(
    uint32_t offset,
    uint32_t value
) {
    MMIO::write32(registers, offset, value);
}

static void barrier() {
    // Compiler ordering fence.
    // The DMA backend must also provide
    // coherent mappings for this platform.
    asm volatile("" ::: "memory");
}

static bool supported(
    const PCI::Device& device
) {
    return device.vendorId == INTEL &&
        (device.deviceId == I82540EM ||
         device.deviceId == I82545EM);
}

static bool find_adapter(
    PCI::Device& output
) {
    for (uint32_t i = 0;
         i < PCI::device_count();
         ++i) {

        PCI::Device d{};

        if (!PCI::device_at(i, d))
            continue;

        if (supported(d)) {
            output = d;
            return true;
        }
    }

    return false;
}

static bool valid_mac(
    const MacAddress& mac
) {
    bool allZero = true;
    bool allFF = true;

    for (uint32_t i = 0; i < 6; ++i) {
        if (mac.bytes[i] != 0)
            allZero = false;

        if (mac.bytes[i] != 0xFF)
            allFF = false;
    }

    return !allZero &&
           !allFF &&
           !(mac.bytes[0] & 1);
}

static void extract_mac(
    uint32_t low,
    uint32_t high
) {
    currentMac.bytes[0] =
        static_cast<uint8_t>(low);

    currentMac.bytes[1] =
        static_cast<uint8_t>(low >> 8);

    currentMac.bytes[2] =
        static_cast<uint8_t>(low >> 16);

    currentMac.bytes[3] =
        static_cast<uint8_t>(low >> 24);

    currentMac.bytes[4] =
        static_cast<uint8_t>(high);

    currentMac.bytes[5] =
        static_cast<uint8_t>(high >> 8);

    macValid =
        (high & (1u << 31)) &&
        valid_mac(currentMac);
}

static void program_mac() {
    uint32_t low =
        static_cast<uint32_t>(currentMac.bytes[0]) |
        (static_cast<uint32_t>(currentMac.bytes[1]) << 8) |
        (static_cast<uint32_t>(currentMac.bytes[2]) << 16) |
        (static_cast<uint32_t>(currentMac.bytes[3]) << 24);

    uint32_t high =
        static_cast<uint32_t>(currentMac.bytes[4]) |
        (static_cast<uint32_t>(currentMac.bytes[5]) << 8) |
        (1u << 31);

    write(REG_RAL0, low);
    write(REG_RAH0, high);
}

// ================================================
// Adapter discovery
// ================================================

bool initialize() {
    detected = false;
    active = false;

    registers = nullptr;
    macValid = false;
    counters = {};

    if (!PCI::initialized() ||
        !MMIO::available())
        return false;

    if (!find_adapter(pciDevice))
        return false;

    PCI::Bar bar{};

    if (!PCI::read_bar(pciDevice, 0, bar))
        return false;

    if (bar.type != PCI::BarType::Memory32 &&
        bar.type != PCI::BarType::Memory64)
        return false;

    registers = MMIO::map(
        bar.address,
        0x6000
    );

    if (!registers)
        return false;

    // Enable PCI MMIO access.
    uint32_t command = PCI::read_config(
        pciDevice.bus,
        pciDevice.slot,
        pciDevice.function,
        0x04
    );

    PCI::write_config(
        pciDevice.bus,
        pciDevice.slot,
        pciDevice.function,
        0x04,
        command | 0x2u
    );

    uint32_t low = read(REG_RAL0);
    uint32_t high = read(REG_RAH0);

    extract_mac(low, high);

    detected = true;

    return true;
}

// ================================================
// DMA allocation
// ================================================

static bool allocate_rings() {
    if (!DMA::available())
        return false;

    if (!DMA::allocate(
        RX_COUNT * sizeof(RxDescriptor),
        128,
        rxRingMemory
    )) {
        return false;
    }

    if (!DMA::allocate(
        TX_COUNT * sizeof(TxDescriptor),
        128,
        txRingMemory
    )) {
        return false;
    }

    DMA::zero(rxRingMemory);
    DMA::zero(txRingMemory);

    rxRing = reinterpret_cast<
        volatile RxDescriptor*
    >(rxRingMemory.virtualAddress);

    txRing = reinterpret_cast<
        volatile TxDescriptor*
    >(txRingMemory.virtualAddress);

    for (uint32_t i = 0; i < RX_COUNT; ++i) {
        if (!DMA::allocate(
            BUFFER_SIZE,
            16,
            rxBuffers[i]
        )) {
            return false;
        }

        DMA::zero(rxBuffers[i]);

        rxRing[i].address =
            rxBuffers[i].physicalAddress;
    }

    for (uint32_t i = 0; i < TX_COUNT; ++i) {
        if (!DMA::allocate(
            BUFFER_SIZE,
            16,
            txBuffers[i]
        )) {
            return false;
        }

        DMA::zero(txBuffers[i]);

        txRing[i].address =
            txBuffers[i].physicalAddress;

        // Descriptor initially available.
        txRing[i].status = TX_DD;
    }

    rxIndex = 0;
    txIndex = 0;

    barrier();

    return true;
}

// ================================================
// NIC reset
// ================================================

static bool reset_adapter() {
    // Mask all device interrupts; this driver
    // currently uses polling.
    write(REG_IMC, 0xFFFFFFFFu);

    write(REG_RCTL, 0);
    write(REG_TCTL, 0);

    uint32_t control = read(REG_CTRL);

    write(REG_CTRL, control | CTRL_RST);

    bool finished = false;

    for (uint32_t i = 0;
         i < RESET_TIMEOUT;
         ++i) {

        if (!(read(REG_CTRL) & CTRL_RST)) {
            finished = true;
            break;
        }

        asm volatile("pause");
    }

    if (!finished)
        return false;

    write(REG_IMC, 0xFFFFFFFFu);

    // Clear pending causes.
    (void)read(REG_ICR);

    // Ask the controller to establish link.
    write(
        REG_CTRL,
        read(REG_CTRL) | CTRL_SLU | CTRL_ASDE
    );

    if (macValid)
        program_mac();

    return true;
}

// ================================================
// Receive and transmit rings
// ================================================

static void configure_receive() {
    const uint64_t physical =
        rxRingMemory.physicalAddress;

    write(
        REG_RDBAL,
        static_cast<uint32_t>(physical)
    );

    write(
        REG_RDBAH,
        static_cast<uint32_t>(physical >> 32)
    );

    write(
        REG_RDLEN,
        RX_COUNT * sizeof(RxDescriptor)
    );

    write(REG_RDH, 0);
    write(REG_RDT, RX_COUNT - 1);

    // 2048-byte receive buffers.
    // Accept broadcast frames and strip FCS.
    write(
        REG_RCTL,
        RCTL_EN | RCTL_BAM | RCTL_SECRC
    );
}

static void configure_transmit() {
    const uint64_t physical =
        txRingMemory.physicalAddress;

    write(
        REG_TDBAL,
        static_cast<uint32_t>(physical)
    );

    write(
        REG_TDBAH,
        static_cast<uint32_t>(physical >> 32)
    );

    write(
        REG_TDLEN,
        TX_COUNT * sizeof(TxDescriptor)
    );

    write(REG_TDH, 0);
    write(REG_TDT, 0);

    // IPG configuration for copper Ethernet.
    write(REG_TIPG, 0x0060200A);

    write(
        REG_TCTL,
        TCTL_EN |
        TCTL_PSP |
        (0x10u << 4) |
        (0x40u << 12)
    );
}

bool start() {
    if (active)
        return true;

    if (!detected || !registers ||
        !macValid)
        return false;

    if (!allocate_rings())
        return false;

    if (!reset_adapter())
        return false;

    // Enable bus mastering only after
    // the DMA descriptors are prepared.
    uint32_t command = PCI::read_config(
        pciDevice.bus,
        pciDevice.slot,
        pciDevice.function,
        0x04
    );

    PCI::write_config(
        pciDevice.bus,
        pciDevice.slot,
        pciDevice.function,
        0x04,
        command | 0x6u
    );

    barrier();

    configure_receive();
    configure_transmit();

    active = true;

    return true;
}

// ================================================
// Driver state
// ================================================

bool initialized() {
    return detected;
}

bool running() {
    return active;
}

bool link_up() {
    if (!detected || !registers)
        return false;

    return (read(REG_STATUS) & STATUS_LU) != 0;
}

bool mac_address(MacAddress& output) {
    if (!detected || !macValid)
        return false;

    output = currentMac;
    return true;
}

const Statistics& statistics() {
    return counters;
}

void set_receive_callback(
    ReceiveCallback callback
) {
    receiveCallback = callback;
}

// ================================================
// Ethernet transmission
// ================================================

bool send(
    const uint8_t* frame,
    uint16_t length
) {
    if (!active || !frame ||
        length < 14 ||
        length > MAX_FRAME)
        return false;

    const uint32_t slot = txIndex;

    volatile TxDescriptor& descriptor =
        txRing[slot];

    // Synchronous polling means we don't
    // reuse the descriptor until completion.
    if (!(descriptor.status & TX_DD))
        return false;

    uint8_t* destination =
        static_cast<uint8_t*>(
            txBuffers[slot].virtualAddress
        );

    for (uint32_t i = 0; i < length; ++i)
        destination[i] = frame[i];

    descriptor.length = length;
    descriptor.checksumOffset = 0;
    descriptor.checksumStart = 0;
    descriptor.special = 0;

    // EOP + request descriptor status writeback.
    descriptor.command = TX_EOP | TX_RS;
    descriptor.status = 0;

    barrier();

    txIndex = (txIndex + 1) % TX_COUNT;

    write(REG_TDT, txIndex);

    // Poll for completion.
    for (uint32_t i = 0;
         i < TX_TIMEOUT;
         ++i) {

        if (descriptor.status & TX_DD) {
            ++counters.transmitted;
            return true;
        }

        asm volatile("pause");
    }

    ++counters.txErrors;
    return false;
}

// ================================================
// Ethernet reception
// ================================================

uint32_t poll(uint32_t budget) {
    if (!active || budget == 0)
        return 0;

    uint32_t processed = 0;

    while (processed < budget) {
        const uint32_t slot = rxIndex;

        volatile RxDescriptor& descriptor =
            rxRing[slot];

        if (!(descriptor.status & RX_DD))
            break;

        barrier();

        const uint16_t length =
            descriptor.length;

        const bool complete =
            (descriptor.status & RX_EOP) != 0;

        const bool error =
            descriptor.errors != 0;

        if (complete && !error &&
            length >= 14 &&
            length <= MAX_FRAME) {

            if (receiveCallback) {
                const uint8_t* data =
                    static_cast<const uint8_t*>(
                        rxBuffers[slot].virtualAddress
                    );

                receiveCallback(data, length);

                ++counters.received;
            } else {
                ++counters.dropped;
            }
        } else {
            // Multi-descriptor and oversized packets
            // aren't supported in this early driver.
            ++counters.dropped;
        }

        descriptor.status = 0;
        descriptor.errors = 0;
        descriptor.length = 0;

        barrier();

        rxIndex = (rxIndex + 1) % RX_COUNT;

        // Return descriptor ownership to NIC.
        write(REG_RDT, slot);

        ++processed;
    }

    return processed;
}

}
