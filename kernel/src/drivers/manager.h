
#pragma once

#include <stdint.h>

namespace Astral::Drivers {

struct Status {
    bool pciReady;
    bool rtcAvailable;
    bool memoryBackendReady;

    bool ethernetDetected;
    bool ethernetReady;
    bool networkReady;
    bool dhcpConfigured;
    bool dnsReady;

    bool wirelessControllerDetected;
    bool wifiDriverReady;

    bool storageControllerDetected;

    bool ahciDetected;
    bool ahciMapped;
    bool ahciReady;

    bool nvmeDetected;
    bool nvmeMapped;
    bool nvmeReady;

    bool usbDetected;
    bool usbMapped;
    bool usbHidReady;

    bool audioControllerDetected;
    bool audioDetected;
    bool audioMapped;
    bool audioPlaybackReady;

    uint32_t pciDeviceCount;
    uint32_t wifiAdapterCount;
};

bool initialize(uint32_t dhcpTransactionId = 0);

bool initialized();

const Status& status();

// Call during the kernel's runtime loop.
//
// elapsedSeconds must be actual elapsed
// whole seconds, not CPU loop iterations.
void poll(
    uint32_t packetBudget = 32,
    uint32_t elapsedSeconds = 0
);

// Convenience accessor retained for code
// using the older driver-manager interface.
bool ethernet_device(
    uint16_t& vendorId,
    uint16_t& deviceId
);

}
