
#pragma once

#include <stdint.h>

namespace Astral::DriverRuntime {

enum class Stage : uint8_t {
    NotStarted,
    Detecting,
    Active,
    Failed
};

struct Status {
    Stage stage;

    bool pciReady;
    bool memoryReady;
    bool networkReady;

    bool storageDetected;
    bool usbDetected;
    bool audioDetected;
    bool wirelessDetected;

    uint32_t pciDevices;
};

bool initialize(uint32_t transactionId = 0);

bool initialized();

const Status& status();

void poll(
    uint32_t packetBudget = 32,
    uint32_t elapsedSeconds = 0
);

}
