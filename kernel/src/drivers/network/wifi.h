
#pragma once

#include <stdint.h>

namespace Astral::WiFi {

enum class State : uint8_t {
    Unavailable,
    Detected,
    DriverRequired,
    Ready,
    Scanning,
    Disconnected,
    Connecting,
    Connected,
    Error
};

enum class Security : uint8_t {
    Open,
    WPA2Personal,
    WPA3Personal,
    Unknown
};

enum class Vendor : uint8_t {
    Unknown,
    Intel,
    QualcommAtheros,
    Broadcom,
    Realtek,
    MediaTek
};

struct Adapter {
    bool present;

    uint8_t bus;
    uint8_t slot;
    uint8_t function;

    uint16_t vendorId;
    uint16_t deviceId;

    Vendor vendor;

    bool driverLoaded;
    bool firmwareLoaded;
};

struct Network {
    char ssid[33];

    uint8_t bssid[6];
    uint8_t signalPercent;

    uint8_t channel;
    Security security;
};

struct Status {
    State state;
    Adapter adapter;

    bool radioEnabled;
    bool scanSupported;
    bool connectionSupported;

    uint32_t discoveredNetworks;
    uint32_t detectedAdapters;
};

// Find PCI wireless controller candidates.
// This does not initialize a radio.
bool initialize();

bool initialized();

const Status& status();

// Number of candidate PCI adapters discovered.
uint32_t adapter_count();

bool adapter_at(
    uint32_t index,
    Adapter& output
);

// Readable manufacturer name.
const char* vendor_name(Vendor vendor);

// Available networks from a future driver.
uint32_t network_count();

bool network_at(
    uint32_t index,
    Network& output
);

// Requires a real chipset driver.
bool scan();

bool connect(
    const char* ssid,
    const char* passphrase
);

void disconnect();

}
