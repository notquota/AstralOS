
#include "manager.h"

#include "../hardware/pci.h"
#include "../hardware/mmio.h"

#include "../memory/dma.h"
#include "../memory/backend.h"

#include "rtc/rtc.h"
#include "network/e1000.h"
#include "network/wifi.h"
#include "storage/ahci.h"
#include "storage/nvme.h"
#include "input/usb_hid.h"
#include "audio/hda.h"

#include "../network/network.h"
#include "../network/ethernet.h"
#include "../network/ipv4.h"
#include "../network/dhcp.h"
#include "../network/dns.h"

namespace Astral::Drivers {

static Status current{};
static bool ready = false;
static bool dnsInitialized = false;

static Network::IPv4Address zero_ip() {
    return Network::IPv4Address{{0, 0, 0, 0}};
}

static void discover_pci() {
    current.pciDeviceCount = PCI::device_count();

    for (uint32_t i = 0;
         i < current.pciDeviceCount;
         ++i) {

        PCI::Device device{};

        if (!PCI::device_at(i, device))
            continue;

        if (device.classCode == 0x02 &&
            device.subclass == 0x00) {
            current.ethernetDetected = true;
        }

        if (device.classCode == 0x02 &&
            device.subclass == 0x80) {
            current.wirelessControllerDetected = true;
        }

        if (device.classCode == 0x01 &&
            device.subclass == 0x06 &&
            device.progIF == 0x01) {
            current.ahciDetected = true;
        }

        if (device.classCode == 0x01 &&
            device.subclass == 0x08 &&
            device.progIF == 0x02) {
            current.nvmeDetected = true;
        }

        if (device.classCode == 0x0C &&
            device.subclass == 0x03) {
            current.usbDetected = true;
        }

        if (device.classCode == 0x04 &&
            device.subclass == 0x03) {
            current.audioDetected = true;
        }
    }

    current.storageControllerDetected =
        current.ahciDetected || current.nvmeDetected;

    current.audioControllerDetected =
        current.audioDetected;
}

static void initialize_storage() {
    // Probe storage controllers, but don't
    // start disk DMA in the global boot path.
    //
    // Disk I/O startup will be tested separately
    // using disposable virtual disks.

    if (current.ahciDetected) {
        AHCI::initialize();
        current.ahciMapped = AHCI::status().mapped;
    }

    if (current.nvmeDetected) {
        NVMe::initialize();
        current.nvmeMapped = NVMe::status().mapped;
    }

    current.ahciReady = AHCI::ready_for_io();
    current.nvmeReady = NVMe::ready_for_io();
}

static void initialize_usb() {
    if (!current.usbDetected)
        return;

    USB::initialize();

    current.usbMapped =
        USB::status().registersMapped;

    current.usbHidReady =
        USB::hid_ready();
}

static void initialize_audio() {
    if (!current.audioDetected)
        return;

    HDA::initialize();

    current.audioMapped =
        HDA::status().mapped;

    current.audioPlaybackReady =
        HDA::playback_ready();
}

static void initialize_wifi() {
    WiFi::initialize();

    current.wifiAdapterCount =
        WiFi::adapter_count();

    current.wirelessControllerDetected =
        current.wifiAdapterCount != 0;

    current.wifiDriverReady =
        WiFi::status().adapter.driverLoaded;
}

static void initialize_network(
    uint32_t transactionId
) {
    if (!MMIO::available() ||
        !DMA::available())
        return;

    // Only the supported Intel E1000 adapter
    // can be started by the current Ethernet
    // implementation.
    if (!E1000::initialize())
        return;

    if (!E1000::start())
        return;

    current.ethernetReady = E1000::running();

    if (!current.ethernetReady)
        return;

    if (!Ethernet::initialize(zero_ip()))
        return;

    IPv4::configure(zero_ip(), zero_ip());

    current.networkReady = true;

    if (transactionId == 0)
        return;

    if (!DHCP::initialize())
        return;

    DHCP::start(transactionId);
}

bool initialize(uint32_t dhcpTransactionId) {
    if (ready)
        return true;

    current = {};
    dnsInitialized = false;

    current.memoryBackendReady =
        MemoryBackend::initialized();

    RTC::DateTime date{};
    current.rtcAvailable = RTC::read(date);

    current.pciReady =
        PCI::initialized() || PCI::initialize();

    if (!current.pciReady)
        return false;

    discover_pci();
    initialize_wifi();

    if (MMIO::available()) {
        initialize_storage();
        initialize_usb();
        initialize_audio();
    }

    // Only attempt bus-mastering Ethernet after
    // the memory backend has been installed.
    if (current.memoryBackendReady)
        initialize_network(dhcpTransactionId);

    ready = true;
    return true;
}

bool initialized() {
    return ready;
}

const Status& status() {
    return current;
}

void poll(
    uint32_t packetBudget,
    uint32_t elapsedSeconds
) {
    if (!ready)
        return;

    if (current.networkReady) {
        if (packetBudget != 0)
            E1000::poll(packetBudget);

        // Prevent an accidental huge catch-up
        // loop if timekeeping is temporarily
        // interrupted.
        if (elapsedSeconds > 60)
            elapsedSeconds = 60;

        for (uint32_t i = 0;
             i < elapsedSeconds;
             ++i) {
            DHCP::tick();

            if (dnsInitialized)
                DNS::tick();
        }

        current.dhcpConfigured =
            DHCP::configured();

        if (current.dhcpConfigured &&
            !dnsInitialized) {

            const auto& lease = DHCP::lease();

            if (!Network::zero_ip(
                lease.dnsServer
            )) {
                dnsInitialized = DNS::initialize(
                    lease.dnsServer
                );
            }
        }

        // DNS configuration is only usable
        // while an IPv4 lease is active.
        current.dnsReady =
            dnsInitialized &&
            current.dhcpConfigured;
    }

    if (USB::initialized() &&
        current.usbMapped) {
        USB::poll();
    }

    current.usbHidReady = USB::hid_ready();

    current.ahciReady = AHCI::ready_for_io();
    current.nvmeReady = NVMe::ready_for_io();
    current.audioPlaybackReady = HDA::playback_ready();
}

bool ethernet_device(
    uint16_t& vendorId,
    uint16_t& deviceId
) {
    if (!current.pciReady)
        return false;

    for (uint32_t i = 0;
         i < PCI::device_count();
         ++i) {
        PCI::Device device{};

        if (!PCI::device_at(i, device))
            continue;

        if (device.vendorId == 0x8086 &&
            (device.deviceId == 0x100E ||
             device.deviceId == 0x100F)) {
            vendorId = device.vendorId;
            deviceId = device.deviceId;
            return true;
        }
    }

    return false;
}

}