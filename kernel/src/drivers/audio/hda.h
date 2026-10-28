
#pragma once

#include <stdint.h>

namespace Astral::HDA {

struct Status {
    bool detected;
    bool mapped;

    uint16_t vendorId;
    uint16_t deviceId;

    uint64_t physicalBar;

    uint8_t majorVersion;
    uint8_t minorVersion;

    uint16_t globalCapabilities;

    uint8_t outputStreams;
    uint8_t inputStreams;
    uint8_t bidirectionalStreams;

    // Additional controller capabilities.
    bool controllerInReset;
    bool supports64BitDma;

    // STATESTS reports codec wake/state events.
    // It is not a definitive codec inventory.
    uint16_t codecStateBits;

    // True only after full audio initialization.
    bool codecInitialized;
    bool streamConfigured;
    bool playbackActive;
};

// Detect an Intel HD Audio-compatible PCI
// controller and inspect its register block.
bool initialize();

bool initialized();

const Status& status();

// Read the current controller and codec-status
// information without modifying hardware.
bool refresh();

// Returns false until the codec command engine,
// audio routing and DMA streams are implemented.
bool playback_ready();

}