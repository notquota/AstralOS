
#pragma once

#include "network.h"

namespace Astral::ICMP {

struct Statistics {
    uint64_t requestsReceived;
    uint64_t repliesSent;
    uint64_t repliesReceived;
    uint64_t invalidPackets;
};

// Called by the IPv4 layer for protocol 1.
void receive(
    Network::IPv4Address source,
    const uint8_t* packet,
    uint16_t length
);

// Send an ICMP Echo Request to another IPv4
// address. The caller handles retries.
bool echo_request(
    Network::IPv4Address destination,
    uint16_t identifier,
    uint16_t sequence
);

const Statistics& statistics();

}
