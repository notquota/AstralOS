
#include "ahci.h"

#include "../../hardware/pci.h"
#include "../../hardware/mmio.h"
#include "../../memory/dma.h"

#include <stdint.h>

namespace Astral::AHCI {

// ==================================================
// AHCI global register offsets
// ==================================================

static constexpr uint32_t REG_CAP  = 0x00;
static constexpr uint32_t REG_GHC  = 0x04;
static constexpr uint32_t REG_PI   = 0x0C;
static constexpr uint32_t REG_VS   = 0x10;
static constexpr uint32_t REG_CAP2 = 0x24;
static constexpr uint32_t REG_BOHC = 0x28;

static constexpr uint32_t GHC_AE = 1u << 31;
static constexpr uint32_t CAP_S64A = 1u << 31;

// ==================================================
// Per-port register offsets
// ==================================================

static constexpr uint32_t PORT_BASE = 0x100;
static constexpr uint32_t PORT_STRIDE = 0x80;

static constexpr uint32_t PX_CLB  = 0x00;
static constexpr uint32_t PX_CLBU = 0x04;
static constexpr uint32_t PX_FB   = 0x08;
static constexpr uint32_t PX_FBU  = 0x0C;
static constexpr uint32_t PX_IS   = 0x10;
static constexpr uint32_t PX_IE   = 0x14;
static constexpr uint32_t PX_CMD  = 0x18;
static constexpr uint32_t PX_TFD  = 0x20;
static constexpr uint32_t PX_SIG  = 0x24;
static constexpr uint32_t PX_SSTS = 0x28;
static constexpr uint32_t PX_SERR = 0x30;
static constexpr uint32_t PX_SACT = 0x34;
static constexpr uint32_t PX_CI   = 0x38;

static constexpr uint32_t CMD_ST  = 1u << 0;
static constexpr uint32_t CMD_FRE = 1u << 4;
static constexpr uint32_t CMD_FR  = 1u << 14;
static constexpr uint32_t CMD_CR  = 1u << 15;

static constexpr uint32_t IS_TFES = 1u << 30;

static constexpr uint32_t SATA_SIG_ATA = 0x00000101;

static constexpr uint8_t ATA_IDENTIFY = 0xEC;
static constexpr uint8_t ATA_READ_DMA_EXT = 0x25;

static constexpr uint32_t ATA_BSY = 0x80;
static constexpr uint32_t ATA_DRQ = 0x08;

static constexpr uint32_t REGISTER_SIZE = 0x1100;
static constexpr uint32_t COMMAND_TIMEOUT = 10000000;

static constexpr uint32_t SECTOR_BYTES = 512;

// ==================================================
// AHCI hardware descriptor structures
// ==================================================

struct CommandHeader {
    uint16_t flags;
    uint16_t prdtLength;
    uint32_t transferredBytes;

    uint32_t tableBaseLow;
    uint32_t tableBaseHigh;

    uint32_t reserved[4];
} __attribute__((packed));

struct PrdtEntry {
    uint32_t dataBaseLow;
    uint32_t dataBaseHigh;
    uint32_t reserved;
    uint32_t byteCount;
} __attribute__((packed));

struct CommandTable {
    uint8_t commandFis[64];
    uint8_t atapiCommand[16];
    uint8_t reserved[48];

    PrdtEntry prdt[1];
} __attribute__((packed));

static_assert(sizeof(CommandHeader) == 32);
static_assert(sizeof(PrdtEntry) == 16);
static_assert(sizeof(CommandTable) == 144);

// ==================================================
// Driver state
// ==================================================

static PCI::Device controller{};

static volatile void* registers = nullptr;

static Status current{};
static Port ports[32]{};

static bool detected = false;
static bool active = false;

static DMA::Buffer commandListMemory{};
static DMA::Buffer receivedFisMemory{};
static DMA::Buffer commandTableMemory{};
static DMA::Buffer sectorMemory{};

static uint32_t activePortBase = 0;

static uint32_t read(uint32_t offset) {
    return MMIO::read32(registers, offset);
}

static void write(
    uint32_t offset,
    uint32_t value
) {
    MMIO::write32(registers, offset, value);
}

static uint32_t port_read(uint32_t offset) {
    return read(activePortBase + offset);
}

static void port_write(
    uint32_t offset,
    uint32_t value
) {
    write(activePortBase + offset, value);
}

static void pause_cpu() {
    asm volatile("pause" ::: "memory");
}

static void memory_barrier() {
    // x86 compiler and hardware ordering barrier.
    // Coherent DMA mappings are still required.
    asm volatile("mfence" ::: "memory");
}

static void copy_bytes(
    uint8_t* destination,
    const uint8_t* source,
    uint32_t length
) {
    for (uint32_t i = 0; i < length; ++i)
        destination[i] = source[i];
}

static uint16_t word_at(
    const uint8_t* bytes,
    uint32_t word
) {
    const uint32_t offset = word * 2;

    return static_cast<uint16_t>(
        bytes[offset] |
        (static_cast<uint16_t>(bytes[offset + 1]) << 8)
    );
}

static bool valid_dma_address(
    const DMA::Buffer& buffer
) {
    if (!DMA::valid(buffer, 1, 2))
        return false;

    // AHCI controllers without 64-bit addressing
    // cannot access DMA memory beyond 4 GiB.
    if (!(current.capabilities & CAP_S64A)) {
        if (buffer.physicalAddress >= 0x100000000ull)
            return false;

        if (buffer.size >
            0x100000000ull - buffer.physicalAddress)
            return false;
    }

    return true;
}

// ==================================================
// PCI discovery
// ==================================================

static bool find_controller(
    PCI::Device& output
) {
    for (uint32_t i = 0; i < PCI::device_count(); ++i) {
        PCI::Device d{};

        if (!PCI::device_at(i, d))
            continue;

        if (d.classCode == 0x01 &&
            d.subclass == 0x06 &&
            d.progIF == 0x01) {
            output = d;
            return true;
        }
    }

    return false;
}

bool initialize() {
    if (active)
        return false;

    detected = false;
    current = {};
    registers = nullptr;

    for (auto& p : ports)
        p = {};

    if (!PCI::initialized() ||
        !MMIO::available())
        return false;

    if (!find_controller(controller))
        return false;

    current.detected = true;

    PCI::Bar bar{};

    if (!PCI::read_bar(controller, 5, bar))
        return false;

    if (bar.type != PCI::BarType::Memory32 &&
        bar.type != PCI::BarType::Memory64)
        return false;

    if (!bar.address)
        return false;

    current.physicalBar = bar.address;

    // Enable PCI memory-space decoding before
    // reading the AHCI controller's MMIO BAR.
    uint32_t pciCommand = PCI::read_config(
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
        pciCommand | 0x2u
    );

    registers = MMIO::map(
        bar.address,
        REGISTER_SIZE
    );

    if (!registers)
        return false;

    current.mapped = true;

    current.capabilities = read(REG_CAP);
    current.version = read(REG_VS);
    current.implementedPorts = read(REG_PI);

    for (uint32_t i = 0; i < 32; ++i) {
        Port& p = ports[i];

        p.index = static_cast<uint8_t>(i);

        p.implemented =
            (current.implementedPorts & (1u << i)) != 0;

        if (!p.implemented)
            continue;

        uint32_t base =
            PORT_BASE + i * PORT_STRIDE;

        p.signature = read(base + PX_SIG);
        p.sataStatus = read(base + PX_SSTS);

        const uint32_t deviceDetection =
            p.sataStatus & 0x0F;

        const uint32_t interfacePower =
            (p.sataStatus >> 8) & 0x0F;

        p.devicePresent =
            deviceDetection == 3 &&
            interfacePower == 1;

        if (p.devicePresent)
            ++current.portsDetected;
    }

    detected = true;
    return true;
}

bool initialized() {
    return detected;
}

const Status& status() {
    return current;
}

bool port_at(
    uint8_t index,
    Port& result
) {
    if (!detected || index >= 32)
        return false;

    result = ports[index];
    return true;
}

// ==================================================
// Command engine control
// ==================================================

static bool wait_command_clear(uint32_t mask) {
    for (uint32_t i = 0;
         i < COMMAND_TIMEOUT;
         ++i) {
        if ((port_read(PX_CMD) & mask) == 0)
            return true;

        pause_cpu();
    }

    return false;
}

static bool stop_engine() {
    uint32_t command = port_read(PX_CMD);

    command &= ~CMD_ST;

    port_write(PX_CMD, command);

    if (!wait_command_clear(CMD_CR))
        return false;

    command = port_read(PX_CMD);
    command &= ~CMD_FRE;

    port_write(PX_CMD, command);

    return wait_command_clear(CMD_FR);
}

static bool start_engine() {
    if (!wait_command_clear(CMD_CR))
        return false;

    uint32_t command = port_read(PX_CMD);

    command |= CMD_FRE;
    port_write(PX_CMD, command);

    command |= CMD_ST;
    port_write(PX_CMD, command);

    return true;
}

// ==================================================
// Allocate DMA command structures
// ==================================================

static bool allocate_command_memory() {
    if (!DMA::available())
        return false;

    // 1024-byte command list, 256-byte received FIS,
    // 128-byte aligned command table.
    //
    // Page-size allocations are used because they
    // work with the existing DMA pool interface.
    if (!DMA::allocate(
        4096,
        4096,
        commandListMemory
    ))
        return false;

    if (!DMA::allocate(
        4096,
        4096,
        receivedFisMemory
    ))
        return false;

    if (!DMA::allocate(
        4096,
        4096,
        commandTableMemory
    ))
        return false;

    if (!DMA::allocate(
        4096,
        4096,
        sectorMemory
    ))
        return false;

    if (!valid_dma_address(commandListMemory) ||
        !valid_dma_address(receivedFisMemory) ||
        !valid_dma_address(commandTableMemory) ||
        !valid_dma_address(sectorMemory))
        return false;

    DMA::zero(commandListMemory);
    DMA::zero(receivedFisMemory);
    DMA::zero(commandTableMemory);
    DMA::zero(sectorMemory);

    return true;
}

// ==================================================
// Command list setup
// ==================================================

static void program_command_memory() {
    const uint64_t list =
        commandListMemory.physicalAddress;

    const uint64_t fis =
        receivedFisMemory.physicalAddress;

    port_write(
        PX_CLB,
        static_cast<uint32_t>(list)
    );

    port_write(
        PX_CLBU,
        static_cast<uint32_t>(list >> 32)
    );

    port_write(
        PX_FB,
        static_cast<uint32_t>(fis)
    );

    port_write(
        PX_FBU,
        static_cast<uint32_t>(fis >> 32)
    );
}

static void prepare_command(
    uint8_t ataCommand,
    uint64_t lba
) {
    DMA::zero(commandListMemory);
    DMA::zero(commandTableMemory);
    DMA::zero(sectorMemory);

    auto* header =
        static_cast<CommandHeader*>(
            commandListMemory.virtualAddress
        );

    auto* table =
        static_cast<CommandTable*>(
            commandTableMemory.virtualAddress
        );

    // FIS length: five 32-bit words.
    header->flags = 5;
    header->prdtLength = 1;

    const uint64_t tablePhysical =
        commandTableMemory.physicalAddress;

    header->tableBaseLow =
        static_cast<uint32_t>(tablePhysical);

    header->tableBaseHigh =
        static_cast<uint32_t>(tablePhysical >> 32);

    PrdtEntry& prdt = table->prdt[0];

    const uint64_t dataPhysical =
        sectorMemory.physicalAddress;

    prdt.dataBaseLow =
        static_cast<uint32_t>(dataPhysical);

    prdt.dataBaseHigh =
        static_cast<uint32_t>(dataPhysical >> 32);

    // PRDT byte count is zero-based.
    prdt.byteCount = SECTOR_BYTES - 1;

    uint8_t* fis = table->commandFis;

    // Register Host-to-Device FIS.
    fis[0] = 0x27;
    fis[1] = 0x80;

    fis[2] = ataCommand;

    if (ataCommand == ATA_READ_DMA_EXT) {
        fis[4] = static_cast<uint8_t>(lba);
        fis[5] = static_cast<uint8_t>(lba >> 8);
        fis[6] = static_cast<uint8_t>(lba >> 16);

        // Device register: LBA mode.
        fis[7] = 0x40;

        fis[8] = static_cast<uint8_t>(lba >> 24);
        fis[9] = static_cast<uint8_t>(lba >> 32);
        fis[10] = static_cast<uint8_t>(lba >> 40);
    }

    // One logical sector.
    fis[12] = 1;
    fis[13] = 0;

    memory_barrier();
}

// ==================================================
// Issue and wait for an ATA command
// ==================================================

static Result execute_command(
    uint8_t ataCommand,
    uint64_t lba
) {
    if (!active)
        return Result::NotInitialized;

    if ((port_read(PX_CI) | port_read(PX_SACT)) != 0)
        return Result::Busy;

    prepare_command(ataCommand, lba);

    // Clear existing interrupt and SATA errors.
    port_write(PX_IS, 0xFFFFFFFFu);
    port_write(PX_SERR, 0xFFFFFFFFu);

    // Wait for the device to become idle.
    bool idle = false;

    for (uint32_t i = 0;
         i < COMMAND_TIMEOUT;
         ++i) {
        if ((port_read(PX_TFD) &
             (ATA_BSY | ATA_DRQ)) == 0) {
            idle = true;
            break;
        }

        pause_cpu();
    }

    if (!idle)
        return Result::Timeout;

    memory_barrier();

    // Issue command in slot zero.
    port_write(PX_CI, 1);

    for (uint32_t i = 0;
         i < COMMAND_TIMEOUT;
         ++i) {

        if (port_read(PX_IS) & IS_TFES)
            return Result::DeviceError;

        if ((port_read(PX_CI) & 1u) == 0) {
            memory_barrier();

            if (port_read(PX_IS) & IS_TFES)
                return Result::DeviceError;

            if (port_read(PX_TFD) & 0x01u)
                return Result::DeviceError;

            return Result::Success;
        }

        pause_cpu();
    }

    return Result::Timeout;
}

// ==================================================
// Disk identification
// ==================================================

static bool identify_disk() {
    const Result result =
        execute_command(ATA_IDENTIFY, 0);

    if (result != Result::Success)
        return false;

    const uint8_t* data =
        static_cast<const uint8_t*>(
            sectorMemory.virtualAddress
        );

    const uint16_t capabilities =
        word_at(data, 83);

    // Require 48-bit LBA support.
    if ((capabilities & (1u << 10)) == 0)
        return false;

    // Logical sector-size information is
    // reported through ATA IDENTIFY words.
    const uint16_t word106 = word_at(data, 106);

    if ((word106 & 0xC000u) == 0x4000u &&
        (word106 & (1u << 12))) {

        const uint32_t logicalWords =
            static_cast<uint32_t>(
                word_at(data, 117)
            ) |
            (static_cast<uint32_t>(
                word_at(data, 118)
            ) << 16);

        // This implementation supports
        // 512-byte sectors only.
        if (logicalWords != 256)
            return false;
    }

    uint64_t sectors = 0;

    for (uint32_t i = 0; i < 4; ++i) {
        sectors |=
            static_cast<uint64_t>(
                word_at(data, 100 + i)
            ) << (i * 16);
    }

    if (sectors == 0)
        return false;

    current.sectorCount = sectors;
    current.sectorSize = SECTOR_BYTES;

    return true;
}

// ==================================================
// Start SATA controller
// ==================================================

bool start() {
    if (active)
        return true;

    if (!detected || !registers)
        return false;

    // Only legacy single-controller polling mode
    // is supported by this implementation.
    //
    // Do not seize ownership if firmware still
    // claims the HBA through BOHC.
    if (read(REG_CAP2) & 1u) {
        const uint32_t bohc = read(REG_BOHC);

        if (bohc & 1u)
            return false;
    }

    // Find an attached ATA disk.
    bool found = false;

    for (uint32_t i = 0; i < 32; ++i) {
        if (!ports[i].implemented ||
            !ports[i].devicePresent ||
            ports[i].signature != SATA_SIG_ATA)
            continue;

        current.activePort =
            static_cast<uint8_t>(i);

        activePortBase =
            PORT_BASE + i * PORT_STRIDE;

        found = true;
        break;
    }

    if (!found)
        return false;

    if (!allocate_command_memory())
        return false;

    // Enable PCI MMIO and bus mastering.
    const uint32_t oldCommand = PCI::read_config(
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
        oldCommand | 0x6u
    );

    // AHCI mode must be active.
    write(REG_GHC, read(REG_GHC) | GHC_AE);

    if (!stop_engine())
        return false;

    program_command_memory();

    // Polling mode: disable port interrupts.
    port_write(PX_IE, 0);

    port_write(PX_IS, 0xFFFFFFFFu);
    port_write(PX_SERR, 0xFFFFFFFFu);

    memory_barrier();

    if (!start_engine())
        return false;

    active = true;

    if (!identify_disk()) {
        active = false;
        current.engineReady = false;
        // Do not free/reuse DMA buffers here:
        // the HBA may still reference them.
        return false;
    }

    current.engineReady = true;
    return true;
}

bool ready_for_io() {
    return active && current.engineReady;
}

// ==================================================
// Read a single SATA sector
// ==================================================

Result read_sector(
    uint64_t lba,
    uint8_t* output,
    uint32_t capacity
) {
    if (!ready_for_io())
        return Result::NotInitialized;

    if (!output || capacity < SECTOR_BYTES)
        return Result::InvalidArgument;

    if (lba >= current.sectorCount ||
        lba >= (1ull << 48))
        return Result::InvalidArgument;

    const Result result =
        execute_command(
            ATA_READ_DMA_EXT,
            lba
        );

    if (result != Result::Success)
        return result;

    const uint8_t* data =
        static_cast<const uint8_t*>(
            sectorMemory.virtualAddress
        );

    copy_bytes(output, data, SECTOR_BYTES);

    return Result::Success;
}

// ==================================================
// Error messages
// ==================================================

const char* result_string(Result result) {
    switch (result) {
        case Result::Success:
            return "Success";

        case Result::NotInitialized:
            return "AHCI not ready";

        case Result::NoDevice:
            return "No SATA device";

        case Result::Unsupported:
            return "Unsupported device";

        case Result::InvalidArgument:
            return "Invalid argument";

        case Result::MemoryUnavailable:
            return "DMA memory unavailable";

        case Result::Busy:
            return "AHCI port busy";

        case Result::Timeout:
            return "AHCI command timed out";

        case Result::DeviceError:
            return "ATA device error";
    }

    return "Unknown AHCI error";
}

}
