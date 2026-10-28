
#include "wifi.h"

#include "../../hardware/pci.h"

#include <stdint.h>

namespace Astral::WiFi {

// ---------------------------------------------
// Limits
// ---------------------------------------------

static constexpr uint32_t MAX_ADAPTERS = 16;
static constexpr uint32_t MAX_NETWORKS = 32;

// ---------------------------------------------
// PCI vendor identifiers
// ---------------------------------------------

static constexpr uint16_t VENDOR_INTEL = 0x8086;
static constexpr uint16_t VENDOR_ATHEROS = 0x168C;
static constexpr uint16_t VENDOR_BROADCOM = 0x14E4;
static constexpr uint16_t VENDOR_REALTEK = 0x10EC;
static constexpr uint16_t VENDOR_MEDIATEK = 0x14C3;
static constexpr uint16_t VENDOR_RALINK = 0x1814;

// ---------------------------------------------
// Driver state
// ---------------------------------------------

static Status current{};

static Adapter adapters[MAX_ADAPTERS]{};
static Network networks[MAX_NETWORKS]{};

static uint32_t adapterCount = 0;
static uint32_t networkCount = 0;

static bool ready = false;

// ---------------------------------------------
// Vendor recognition
// ---------------------------------------------

static Vendor identify_vendor(uint16_t id) {
    switch (id) {
        case VENDOR_INTEL:
            return Vendor::Intel;

        case VENDOR_ATHEROS:
            return Vendor::QualcommAtheros;

        case VENDOR_BROADCOM:
            return Vendor::Broadcom;

        case VENDOR_REALTEK:
            return Vendor::Realtek;

        case VENDOR_MEDIATEK:
        case VENDOR_RALINK:
            return Vendor::MediaTek;

        default:
            return Vendor::Unknown;
    }
}

const char* vendor_name(Vendor vendor) {
    switch (vendor) {
        case Vendor::Intel:
            return "Intel";

        case Vendor::QualcommAtheros:
            return "Qualcomm Atheros";

        case Vendor::Broadcom:
            return "Broadcom";

        case Vendor::Realtek:
            return "Realtek";

        case Vendor::MediaTek:
            return "MediaTek";

        default:
            return "Unknown";
    }
}

// ---------------------------------------------
// PCI classification
// ---------------------------------------------

static bool is_candidate(
    const PCI::Device& device
) {
    if (device.classCode != 0x02)
        return false;

    // PCI subclass 0x80 is "Other Network
    // Controller", not exclusively Wi-Fi.
    //
    // A wireless chipset still needs to be
    // confirmed using vendor/device IDs.
    return device.subclass == 0x80;
}

// ---------------------------------------------
// State management
// ---------------------------------------------

static void clear_state() {
    current = {};
    current.state = State::Unavailable;

    adapterCount = 0;
    networkCount = 0;

    for (uint32_t i = 0; i < MAX_ADAPTERS; ++i)
        adapters[i] = {};

    for (uint32_t i = 0; i < MAX_NETWORKS; ++i)
        networks[i] = {};
}

// ---------------------------------------------
// PCI discovery
// ---------------------------------------------

bool initialize() {
    clear_state();
    ready = false;

    if (!PCI::initialized())
        return false;

    for (uint32_t i = 0;
         i < PCI::device_count();
         ++i) {

        PCI::Device device{};

        if (!PCI::device_at(i, device))
            continue;

        if (!is_candidate(device))
            continue;

        if (adapterCount >= MAX_ADAPTERS)
            break;

        Adapter& adapter =
            adapters[adapterCount++];

        adapter.present = true;

        adapter.bus = device.bus;
        adapter.slot = device.slot;
        adapter.function = device.function;

        adapter.vendorId = device.vendorId;
        adapter.deviceId = device.deviceId;

        adapter.vendor =
            identify_vendor(device.vendorId);

        // No hardware-specific driver has
        // been implemented or registered.
        adapter.driverLoaded = false;
        adapter.firmwareLoaded = false;
    }

    current.detectedAdapters = adapterCount;

    if (adapterCount == 0) {
        current.state = State::Unavailable;
        ready = true;
        return true;
    }

    // The first candidate is selected for
    // display in the Settings interface.
    current.adapter = adapters[0];

    // All current wireless candidates need
    // a chipset-specific driver.
    current.state = State::DriverRequired;

    current.radioEnabled = false;
    current.scanSupported = false;
    current.connectionSupported = false;

    current.discoveredNetworks = 0;

    ready = true;
    return true;
}

bool initialized() {
    return ready;
}

const Status& status() {
    return current;
}

uint32_t adapter_count() {
    return adapterCount;
}

bool adapter_at(
    uint32_t index,
    Adapter& output
) {
    if (!ready || index >= adapterCount)
        return false;

    output = adapters[index];
    return true;
}

// ---------------------------------------------
// Discovered Wi-Fi networks
// ---------------------------------------------

uint32_t network_count() {
    return networkCount;
}

bool network_at(
    uint32_t index,
    Network& output
) {
    if (!ready || index >= networkCount)
        return false;

    output = networks[index];
    return true;
}

// ---------------------------------------------
// Wireless operations
// ---------------------------------------------

bool scan() {
    if (!ready ||
        !current.adapter.present ||
        !current.scanSupported)
        return false;

    // No radio backend is installed.
    return false;
}

bool connect(
    const char* ssid,
    const char* passphrase
) {
    if (!ssid || !*ssid)
        return false;

    (void)passphrase;

    if (!ready ||
        !current.adapter.present ||
        !current.connectionSupported)
        return false;

    // Association, encryption and firmware
    // interfaces are not yet implemented.
    return false;
}

void disconnect() {
    networkCount = 0;

    for (uint32_t i = 0; i < MAX_NETWORKS; ++i)
        networks[i] = {};

    current.discoveredNetworks = 0;

    if (!ready)
        return;

    current.state = current.adapter.present
        ? State::DriverRequired
        : State::Unavailable;
}

}