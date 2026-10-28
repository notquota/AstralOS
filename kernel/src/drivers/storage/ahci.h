
#pragma once

#include <stdint.h>

namespace Astral::AHCI {

struct Port {
    uint8_t index;
    bool implemented;
    bool devicePresent;

    uint32_t signature;
    uint32_t sataStatus;
};

struct Status {
    bool detected;
    bool mapped;

    uint32_t version;
    uint32_t capabilities;
    uint32_t implementedPorts;
    uint32_t portsDetected;

    uint64_t physicalBar;

    bool engineReady;
    uint8_t activePort;

    uint64_t sectorCount;
    uint32_t sectorSize;
};

enum class Result : uint8_t {
    Success,
    NotInitialized,
    NoDevice,
    Unsupported,
    InvalidArgument,
    MemoryUnavailable,
    Busy,
    Timeout,
    DeviceError
};

// Read-only PCI/AHCI hardware detection.
bool initialize();

bool initialized();

const Status& status();

bool port_at(
    uint8_t index,
    Port& result
);

// Explicitly initialize one SATA disk's
// command engine and issue ATA IDENTIFY.
//
// Requires MMIO and DMA to be initialized.
bool start();

bool ready_for_io();

// Read one 512-byte logical sector from
// the selected SATA disk.
//
// Caller must provide at least 512 bytes.
// Only supported after start() succeeds.
Result read_sector(
    uint64_t lba,
    uint8_t* output,
    uint32_t capacity
);

const char* result_string(Result result);

}
