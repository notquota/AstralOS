
#pragma once

#include <stdint.h>

namespace Astral::NVMe {

enum class Result : uint8_t {
    Success,
    NotInitialized,
    NoController,
    NoNamespace,
    Unsupported,
    InvalidArgument,
    MemoryUnavailable,
    Busy,
    Timeout,
    ControllerError,
    CommandError
};

struct Namespace {
    uint32_t identifier;
    uint64_t sectorCount;
    uint32_t sectorSize;
    bool active;
};

struct Status {
    bool detected;
    bool mapped;
    bool controllerReady;
    bool adminQueueReady;
    bool ioQueueReady;
    bool engineReady;

    uint16_t vendorId;
    uint16_t deviceId;

    uint64_t physicalBar;
    uint64_t capabilities;

    uint32_t version;
    uint32_t controllerStatus;

    uint32_t minimumPageSize;
    uint32_t maximumPageSize;
    uint32_t maximumQueueEntries;

    uint32_t namespaceCount;
    Namespace activeNamespace;
};

// Non-destructive PCI and NVMe register probe.
bool initialize();

bool initialized();

const Status& status();

// Explicitly initialize controller queues,
// identify namespaces and enable read-only I/O.
//
// Requires working MMIO and DMA backends.
bool start();

bool ready_for_io();

Result read_sector(
    uint64_t lba,
    uint8_t* output,
    uint32_t capacity
);

const char* result_string(Result result);

}
