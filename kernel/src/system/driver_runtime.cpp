
#include "driver_runtime.h"

#include "../drivers/manager.h"
#include "../memory/backend.h"

namespace Astral::DriverRuntime {

static Status current{};
static bool started = false;

bool initialize(uint32_t transactionId) {
    if (started)
        return true;

    current = {};
    current.stage = Stage::Detecting;

    if (!Drivers::initialize(transactionId)) {
        current.stage = Stage::Failed;
        return false;
    }

    const Drivers::Status& drivers =
        Drivers::status();

    current.pciReady = drivers.pciReady;

    current.memoryReady =
        MemoryBackend::initialized();

    current.networkReady =
        drivers.networkReady;

    current.storageDetected =
        drivers.storageControllerDetected;

    current.usbDetected =
        drivers.usbDetected;

    current.audioDetected =
        drivers.audioControllerDetected;

    current.wirelessDetected =
        drivers.wirelessControllerDetected;

    current.pciDevices =
        drivers.pciDeviceCount;

    current.stage = Stage::Active;
    started = true;

    return true;
}

bool initialized() {
    return started;
}

const Status& status() {
    return current;
}

void poll(
    uint32_t packetBudget,
    uint32_t elapsedSeconds
) {
    if (!started)
        return;

    Drivers::poll(
        packetBudget,
        elapsedSeconds
    );

    current.networkReady =
        Drivers::status().networkReady;
}

}
