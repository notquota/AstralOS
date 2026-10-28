
#include "nvme.h"

#include "../../hardware/pci.h"
#include "../../hardware/mmio.h"
#include "../../memory/dma.h"

#include <stdint.h>

namespace Astral::NVMe {

// ================================================
// NVMe register offsets
// ================================================

static constexpr uint32_t REG_CAP_LO = 0x00;
static constexpr uint32_t REG_CAP_HI = 0x04;
static constexpr uint32_t REG_VS     = 0x08;

static constexpr uint32_t REG_CC     = 0x14;
static constexpr uint32_t REG_CSTS   = 0x1C;

static constexpr uint32_t REG_AQA    = 0x24;
static constexpr uint32_t REG_ASQ_LO = 0x28;
static constexpr uint32_t REG_ASQ_HI = 0x2C;
static constexpr uint32_t REG_ACQ_LO = 0x30;
static constexpr uint32_t REG_ACQ_HI = 0x34;

static constexpr uint32_t DOORBELL_BASE = 0x1000;

static constexpr uint32_t CC_EN = 1u << 0;
static constexpr uint32_t CSTS_RDY = 1u << 0;
static constexpr uint32_t CSTS_CFS = 1u << 1;

// ================================================
// NVMe command opcodes
// ================================================

static constexpr uint8_t ADMIN_CREATE_SQ = 0x01;
static constexpr uint8_t ADMIN_CREATE_CQ = 0x05;
static constexpr uint8_t ADMIN_IDENTIFY  = 0x06;

static constexpr uint8_t IO_READ = 0x02;

// ================================================
// Driver limits
// ================================================

static constexpr uint16_t QUEUE_DEPTH = 16;

static constexpr uint32_t PAGE_BYTES = 4096;
static constexpr uint32_t SECTOR_BYTES = 512;

static constexpr uint32_t COMMAND_TIMEOUT = 10000000;
static constexpr uint32_t READY_TIMEOUT = 10000000;

static constexpr uint16_t PCI_NVME_VENDOR_CLASS = 0x01;
static constexpr uint16_t PCI_NVME_SUBCLASS = 0x08;
static constexpr uint16_t PCI_NVME_PROGIF = 0x02;

// ================================================
// NVMe command / completion structures
// ================================================

struct Submission {
    uint32_t commandDword0;
    uint32_t namespaceId;

    uint64_t reserved;
    uint64_t metadata;

    uint64_t prp1;
    uint64_t prp2;

    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} __attribute__((packed));

struct Completion {
    uint32_t result;
    uint32_t reserved;

    uint16_t submissionHead;
    uint16_t submissionQueueId;
    uint16_t commandId;
    uint16_t status;
} __attribute__((packed));

static_assert(sizeof(Submission) == 64);
static_assert(sizeof(Completion) == 16);

// ================================================
// Queue metadata
// ================================================

struct Queue {
    DMA::Buffer submissionMemory;
    DMA::Buffer completionMemory;

    volatile Submission* submissions;
    volatile Completion* completions;

    uint16_t identifier;
    uint16_t tail;
    uint16_t head;
    uint16_t nextCommandId;

    uint8_t phase;
    bool configured;
};

// ================================================
// Driver state
// ================================================

static PCI::Device pciDevice{};

static volatile void* registers = nullptr;

static Status current{};
static Namespace namespaceInfo{};

static bool detected = false;
static bool running = false;
static bool faulted = false;

static uint32_t doorbellStride = 4;

static Queue adminQueue{};
static Queue ioQueue{};

static DMA::Buffer identifyMemory{};
static DMA::Buffer transferMemory{};

// ================================================
// Low-level helpers
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

static void pause_cpu() {
    asm volatile("pause" ::: "memory");
}

static void memory_barrier() {
    // This is for x86-64 with coherent DMA
    // allocations. Other architectures need
    // architecture-specific DMA barriers.
    asm volatile("mfence" ::: "memory");
}

static uint32_t read_le32(
    const uint8_t* p
) {
    return
        static_cast<uint32_t>(p[0]) |
        (static_cast<uint32_t>(p[1]) << 8) |
        (static_cast<uint32_t>(p[2]) << 16) |
        (static_cast<uint32_t>(p[3]) << 24);
}

static uint64_t read_le64(
    const uint8_t* p
) {
    return
        static_cast<uint64_t>(read_le32(p)) |
        (static_cast<uint64_t>(
            read_le32(p + 4)
        ) << 32);
}

static void copy_bytes(
    uint8_t* destination,
    const uint8_t* source,
    uint32_t size
) {
    for (uint32_t i = 0; i < size; ++i)
        destination[i] = source[i];
}

static uint64_t read_capabilities() {
    const uint64_t low = read(REG_CAP_LO);
    const uint64_t high = read(REG_CAP_HI);

    return low | (high << 32);
}

// ================================================
// PCI controller discovery
// ================================================

static bool find_controller(
    PCI::Device& output
) {
    for (uint32_t i = 0;
         i < PCI::device_count();
         ++i) {

        PCI::Device device{};

        if (!PCI::device_at(i, device))
            continue;

        if (device.classCode ==
                PCI_NVME_VENDOR_CLASS &&
            device.subclass ==
                PCI_NVME_SUBCLASS &&
            device.progIF ==
                PCI_NVME_PROGIF) {

            output = device;
            return true;
        }
    }

    return false;
}

// ================================================
// Read-only controller detection
// ================================================

bool initialize() {
    if (running)
        return false;

    current = {};
    namespaceInfo = {};

    detected = false;
    faulted = false;
    registers = nullptr;

    adminQueue = {};
    ioQueue = {};

    if (!PCI::initialized() ||
        !MMIO::available())
        return false;

    if (!find_controller(pciDevice))
        return false;

    current.detected = true;

    current.vendorId = pciDevice.vendorId;
    current.deviceId = pciDevice.deviceId;

    PCI::Bar bar{};

    if (!PCI::read_bar(pciDevice, 0, bar))
        return false;

    if (bar.type != PCI::BarType::Memory32 &&
        bar.type != PCI::BarType::Memory64)
        return false;

    if (bar.address == 0)
        return false;

    current.physicalBar = bar.address;

    // Enable PCI memory-space decoding.
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

    // Obtain CAP first. Initial mapping must
    // contain controller registers and doorbells.
    registers = MMIO::map(
        bar.address,
        0x1000
    );

    if (!registers)
        return false;

    current.capabilities = read_capabilities();
    current.version = read(REG_VS);
    current.controllerStatus = read(REG_CSTS);

    current.controllerReady =
        (current.controllerStatus & CSTS_RDY) != 0;

    // CAP.MQES: zero-based maximum queue entries.
    current.maximumQueueEntries =
        static_cast<uint32_t>(
            current.capabilities & 0xFFFF
        ) + 1;

    const uint32_t dstrd =
        static_cast<uint32_t>(
            (current.capabilities >> 32) & 0x0F
        );

    doorbellStride = 4u << dstrd;

    const uint32_t minShift =
        static_cast<uint32_t>(
            (current.capabilities >> 48) & 0x0F
        );

    const uint32_t maxShift =
        static_cast<uint32_t>(
            (current.capabilities >> 52) & 0x0F
        );

    current.minimumPageSize =
        PAGE_BYTES << minShift;

    current.maximumPageSize =
        PAGE_BYTES << maxShift;

    // Map another range containing the I/O
    // queue's submission/completion doorbells.
    //
    // The MMIO allocator intentionally
    // returns independent virtual mappings.
    const uint64_t doorbellBytes =
        DOORBELL_BASE +
        static_cast<uint64_t>(4) *
        doorbellStride +
        4;

    volatile void* fullRegisters = MMIO::map(
        bar.address,
        doorbellBytes
    );

    if (!fullRegisters)
        return false;

    registers = fullRegisters;

    current.mapped = true;
    detected = true;

    return true;
}

bool initialized() {
    return detected;
}

const Status& status() {
    return current;
}

// ================================================
// DMA allocation
// ================================================

static bool allocate_queue(
    Queue& queue,
    uint16_t id
) {
    queue = {};

    if (!DMA::allocate(
        PAGE_BYTES,
        PAGE_BYTES,
        queue.submissionMemory
    )) {
        return false;
    }

    if (!DMA::allocate(
        PAGE_BYTES,
        PAGE_BYTES,
        queue.completionMemory
    )) {
        return false;
    }

    DMA::zero(queue.submissionMemory);
    DMA::zero(queue.completionMemory);

    queue.submissions =
        reinterpret_cast<volatile Submission*>(
            queue.submissionMemory.virtualAddress
        );

    queue.completions =
        reinterpret_cast<volatile Completion*>(
            queue.completionMemory.virtualAddress
        );

    queue.identifier = id;
    queue.tail = 0;
    queue.head = 0;
    queue.nextCommandId = 1;
    queue.phase = 1;
    queue.configured = true;

    return true;
}

static bool allocate_resources() {
    if (!DMA::available())
        return false;

    if (!allocate_queue(adminQueue, 0))
        return false;

    if (!allocate_queue(ioQueue, 1))
        return false;

    if (!DMA::allocate(
        PAGE_BYTES,
        PAGE_BYTES,
        identifyMemory
    )) {
        return false;
    }

    if (!DMA::allocate(
        PAGE_BYTES,
        PAGE_BYTES,
        transferMemory
    )) {
        return false;
    }

    DMA::zero(identifyMemory);
    DMA::zero(transferMemory);

    return true;
}

// ================================================
// Doorbells
// ================================================

static uint32_t submission_doorbell(
    const Queue& queue
) {
    return DOORBELL_BASE +
        static_cast<uint32_t>(
            2 * queue.identifier
        ) * doorbellStride;
}

static uint32_t completion_doorbell(
    const Queue& queue
) {
    return DOORBELL_BASE +
        static_cast<uint32_t>(
            2 * queue.identifier + 1
        ) * doorbellStride;
}

// ================================================
// Controller ready-state handling
// ================================================

static bool wait_ready(bool expected) {
    for (uint32_t i = 0;
         i < READY_TIMEOUT;
         ++i) {

        uint32_t status = read(REG_CSTS);

        if (status & CSTS_CFS)
            return false;

        bool readyBit =
            (status & CSTS_RDY) != 0;

        if (readyBit == expected)
            return true;

        pause_cpu();
    }

    return false;
}

static bool disable_controller() {
    uint32_t cc = read(REG_CC);

    if (cc & CC_EN)
        write(REG_CC, cc & ~CC_EN);

    return wait_ready(false);
}

static bool enable_controller() {
    // CC:
    // CSS = 0   (NVM command set)
    // MPS = 0   (4096-byte pages)
    // IOSQES = 6 (64 bytes)
    // IOCQES = 4 (16 bytes)
    // EN = 1
    uint32_t cc =
        (6u << 16) |
        (4u << 20) |
        CC_EN;

    write(REG_CC, cc);

    return wait_ready(true);
}

// ================================================
// Configure admin queues
// ================================================

static void setup_admin_queue() {
    // AQA contains zero-based queue depths.
    uint32_t aqa =
        (static_cast<uint32_t>(
            QUEUE_DEPTH - 1
        ) << 16) |
        (QUEUE_DEPTH - 1);

    write(REG_AQA, aqa);

    const uint64_t asq =
        adminQueue.submissionMemory.physicalAddress;

    const uint64_t acq =
        adminQueue.completionMemory.physicalAddress;

    write(REG_ASQ_LO, static_cast<uint32_t>(asq));
    write(REG_ASQ_HI, static_cast<uint32_t>(asq >> 32));

    write(REG_ACQ_LO, static_cast<uint32_t>(acq));
    write(REG_ACQ_HI, static_cast<uint32_t>(acq >> 32));
}

// ================================================
// Submit one command and wait for completion
// ================================================

static Result execute(
    Queue& queue,
    const Submission& command
) {
    if (faulted || !queue.configured)
        return Result::NotInitialized;

    const uint16_t slot = queue.tail;

    const uint16_t commandId =
        queue.nextCommandId++;

    Submission prepared = command;

    prepared.commandDword0 &=
        0x0000FFFFu;

    prepared.commandDword0 |=
        static_cast<uint32_t>(
            commandId
        ) << 16;

    // Copy to volatile DMA queue.
    const uint8_t* source =
        reinterpret_cast<const uint8_t*>(
            &prepared
        );

    volatile uint8_t* destination =
        reinterpret_cast<volatile uint8_t*>(
            &queue.submissions[slot]
        );

    for (uint32_t i = 0;
         i < sizeof(Submission);
         ++i) {
        destination[i] = source[i];
    }

    memory_barrier();

    queue.tail =
        static_cast<uint16_t>(
            (queue.tail + 1) % QUEUE_DEPTH
        );

    write(
        submission_doorbell(queue),
        queue.tail
    );

    // One outstanding command per queue.
    for (uint32_t i = 0;
         i < COMMAND_TIMEOUT;
         ++i) {

        volatile Completion& completion =
            queue.completions[queue.head];

        uint16_t status = completion.status;

        if ((status & 1u) != queue.phase) {
            pause_cpu();
            continue;
        }

        memory_barrier();

        uint16_t completedId =
            completion.commandId;

        uint16_t completedQueue =
            completion.submissionQueueId;

        uint16_t completedStatus =
            completion.status;

        queue.head =
            static_cast<uint16_t>(
                (queue.head + 1) % QUEUE_DEPTH
            );

        if (queue.head == 0)
            queue.phase ^= 1;

        write(
            completion_doorbell(queue),
            queue.head
        );

        if (completedId != commandId ||
            completedQueue != queue.identifier) {
            faulted = true;
            return Result::ControllerError;
        }

        // Bits 1..15 contain completion status.
        if ((completedStatus >> 1) != 0)
            return Result::CommandError;

        return Result::Success;
    }

    // After a timeout, an outstanding command
    // may still be executing. Never reuse its
    // DMA memory until controller recovery.
    faulted = true;
    return Result::Timeout;
}

// ================================================
// Admin command helpers
// ================================================

static Result identify(
    uint8_t cns,
    uint32_t namespaceId
) {
    DMA::zero(identifyMemory);

    Submission command{};

    command.commandDword0 = ADMIN_IDENTIFY;
    command.namespaceId = namespaceId;

    command.prp1 =
        identifyMemory.physicalAddress;

    command.cdw10 = cns;

    return execute(adminQueue, command);
}

static bool create_io_completion_queue() {
    Submission command{};

    command.commandDword0 =
        ADMIN_CREATE_CQ;

    command.prp1 =
        ioQueue.completionMemory.physicalAddress;

    command.cdw10 =
        1u |
        (static_cast<uint32_t>(
            QUEUE_DEPTH - 1
        ) << 16);

    // Physically contiguous, polling mode.
    // Interrupt-enable bit remains clear.
    command.cdw11 = 1u;

    return execute(
        adminQueue,
        command
    ) == Result::Success;
}

static bool create_io_submission_queue() {
    Submission command{};

    command.commandDword0 =
        ADMIN_CREATE_SQ;

    command.prp1 =
        ioQueue.submissionMemory.physicalAddress;

    command.cdw10 =
        1u |
        (static_cast<uint32_t>(
            QUEUE_DEPTH - 1
        ) << 16);

    // Physically contiguous, assigned to CQ1.
    command.cdw11 =
        1u |
        (1u << 16);

    return execute(
        adminQueue,
        command
    ) == Result::Success;
}

// ================================================
// Namespace discovery
// ================================================

static bool discover_namespace() {
    // Identify Controller: contains NN at
    // byte offset 516.
    if (identify(1, 0) != Result::Success)
        return false;

    const uint8_t* controllerData =
        static_cast<const uint8_t*>(
            identifyMemory.virtualAddress
        );

    const uint32_t count =
        read_le32(controllerData + 516);

    if (count == 0)
        return false;

    current.namespaceCount = count;

    // Identify Active Namespace ID List.
    // CNS=2, NSID=0.
    if (identify(2, 0) != Result::Success)
        return false;

    const uint8_t* list =
        static_cast<const uint8_t*>(
            identifyMemory.virtualAddress
        );

    uint32_t namespaceId = read_le32(list);

    if (!namespaceId)
        return false;

    // Identify Namespace.
    if (identify(0, namespaceId) != Result::Success)
        return false;

    const uint8_t* data =
        static_cast<const uint8_t*>(
            identifyMemory.virtualAddress
        );

    uint64_t sectorCount = read_le64(data + 0);

    if (!sectorCount)
        return false;

    // FLBAS is at byte offset 26.
    // This prototype supports the basic
    // low-four-bit LBA format selection.
    const uint8_t formatIndex = data[26] & 0x0F;

    // NLBAF (zero-based) is byte 25.
    if (formatIndex > data[25])
        return false;

    // Each LBA format occupies four bytes,
    // beginning at offset 128.
    const uint32_t formatOffset =
        128 + formatIndex * 4;

    const uint8_t metadataLow =
        data[formatOffset];

    const uint8_t metadataHigh =
        data[formatOffset + 1];

    const uint8_t sectorShift =
        data[formatOffset + 2];

    // No metadata and 512-byte LBA only.
    if (metadataLow != 0 ||
        metadataHigh != 0 ||
        sectorShift != 9)
        return false;

    namespaceInfo.identifier = namespaceId;
    namespaceInfo.sectorCount = sectorCount;
    namespaceInfo.sectorSize = SECTOR_BYTES;
    namespaceInfo.active = true;

    current.activeNamespace = namespaceInfo;

    return true;
}

// ================================================
// Start NVMe controller
// ================================================

bool start() {
    if (running)
        return true;

    if (!detected ||
        !registers ||
        !DMA::available() ||
        faulted)
        return false;

    if (current.maximumQueueEntries < QUEUE_DEPTH)
        return false;

    // This implementation uses 4096-byte pages.
    if (current.minimumPageSize > PAGE_BYTES ||
        current.maximumPageSize < PAGE_BYTES)
        return false;

    // Require NVM command set support.
    // CAP.CSS bit zero corresponds to the
    // NVM command set.
    if ((current.capabilities & (1ull << 37)) == 0)
        return false;

    if (!allocate_resources())
        return false;

    // Enable PCI bus mastering before
    // starting the controller.
    uint32_t pciCommand = PCI::read_config(
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
        pciCommand | 0x6u
    );

    // Administrative queue reset.
    if (!disable_controller())
        return false;

    setup_admin_queue();

    memory_barrier();

    if (!enable_controller())
        return false;

    current.controllerReady = true;
    current.adminQueueReady = true;

    if (!discover_namespace())
        return false;

    if (!create_io_completion_queue())
        return false;

    if (!create_io_submission_queue())
        return false;

    current.ioQueueReady = true;

    current.controllerStatus = read(REG_CSTS);

    current.engineReady = true;
    running = true;

    return true;
}

bool ready_for_io() {
    return running &&
           current.engineReady &&
           namespaceInfo.active &&
           !faulted;
}

// ================================================
// Read a 512-byte logical sector
// ================================================

Result read_sector(
    uint64_t lba,
    uint8_t* output,
    uint32_t capacity
) {
    if (!ready_for_io())
        return Result::NotInitialized;

    if (!output || capacity < SECTOR_BYTES)
        return Result::InvalidArgument;

    if (lba >= namespaceInfo.sectorCount)
        return Result::InvalidArgument;

    DMA::zero(transferMemory);

    Submission command{};

    command.commandDword0 = IO_READ;
    command.namespaceId =
        namespaceInfo.identifier;

    // One 512-byte block fits inside one
    // page-aligned DMA buffer.
    command.prp1 =
        transferMemory.physicalAddress;

    command.cdw10 =
        static_cast<uint32_t>(lba);

    command.cdw11 =
        static_cast<uint32_t>(lba >> 32);

    // NLB is zero-based: zero means one block.
    command.cdw12 = 0;

    const Result result =
        execute(ioQueue, command);

    if (result != Result::Success)
        return result;

    memory_barrier();

    const uint8_t* source =
        static_cast<const uint8_t*>(
            transferMemory.virtualAddress
        );

    copy_bytes(
        output,
        source,
        SECTOR_BYTES
    );

    return Result::Success;
}

// ================================================
// Error messages
// ================================================

const char* result_string(Result result) {
    switch (result) {
        case Result::Success:
            return "Success";

        case Result::NotInitialized:
            return "NVMe not initialized";

        case Result::NoController:
            return "No NVMe controller";

        case Result::NoNamespace:
            return "No supported NVMe namespace";

        case Result::Unsupported:
            return "Unsupported NVMe configuration";

        case Result::InvalidArgument:
            return "Invalid argument";

        case Result::MemoryUnavailable:
            return "DMA memory unavailable";

        case Result::Busy:
            return "NVMe queue busy";

        case Result::Timeout:
            return "NVMe command timeout";

        case Result::ControllerError:
            return "NVMe controller error";

        case Result::CommandError:
            return "NVMe command failed";
    }

    return "Unknown NVMe error";
}

}
