
#pragma once

#include <stdint.h>
#include "network.h"

namespace Astral::TCP {

enum class State : uint8_t {
    Closed,
    SynSent,
    Established,
    Closing,
    Error
};

using Transmit = bool (*)(
    Network::IPv4Address destination,
    const uint8_t* segment,
    uint16_t length
);

using Receive = void (*)(
    const uint8_t* data,
    uint16_t length
);

void initialize(
    Network::IPv4Address local,
    Transmit transmit
);

void set_receive_callback(Receive callback);

bool connect(
    Network::IPv4Address remote,
    uint16_t remotePort,
    uint16_t localPort,
    uint32_t initialSequence
);

bool send(
    const uint8_t* data,
    uint16_t length
);

bool close();

void receive(
    Network::IPv4Address source,
    Network::IPv4Address destination,
    const uint8_t* segment,
    uint16_t length
);

void poll(uint64_t milliseconds);

State state();
bool busy();

}