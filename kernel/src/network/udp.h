
#pragma once

#include "network.h"

namespace Astral::UDP {

using Handler = void (*)(
    Network::IPv4Address source,
    uint16_t sourcePort,
    const uint8_t* data,
    uint16_t length
);

bool bind(uint16_t port, Handler handler);
void unbind(uint16_t port);

bool send(
    Network::IPv4Address destination,
    uint16_t sourcePort,
    uint16_t destinationPort,
    const uint8_t* data,
    uint16_t length
);

void receive(
    Network::IPv4Address source,
    Network::IPv4Address destination,
    const uint8_t* packet,
    uint16_t length
);

}
