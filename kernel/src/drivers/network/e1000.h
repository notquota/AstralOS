
#pragma once

#include <stdint.h>

#include "../../hardware/pci.h"

namespace Astral::E1000 {

struct MacAddress {
    uint8_t bytes[6];
};

struct Statistics {
    uint64_t transmitted;
    uint64_t received;
    uint64_t dropped;
    uint64_t txErrors;
};

// Packet data belongs to the driver only for
// the duration of the callback.
// Copy it if it must be retained.
using ReceiveCallback = void (*)(
    const uint8_t* frame,
    uint16_t length
);

// Find and inspect the supported E1000 adapter.
// Requires PCI + MMIO mapping.
bool initialize();

// Set up physical DMA rings and enable the NIC.
// Requires DMA allocator installation.
bool start();

bool initialized();
bool running();

bool link_up();

bool mac_address(MacAddress& output);

// Synchronous, polled transmission.
// No asynchronous queue or interrupt needed.
bool send(
    const uint8_t* frame,
    uint16_t length
);

// Process up to budget received descriptors.
// Call regularly from the future network loop.
uint32_t poll(uint32_t budget = 32);

void set_receive_callback(
    ReceiveCallback callback
);

const Statistics& statistics();

}
