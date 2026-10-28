
#pragma once

#include "network.h"

namespace Astral::ARP {

void initialize();

void receive(
    const uint8_t* packet,
    uint16_t length
);

// Return a known mapping if one exists.
bool lookup(
    Network::IPv4Address ip,
    Network::MacAddress& mac
);

// Add a mapping learned from a packet.
void remember(
    Network::IPv4Address ip,
    Network::MacAddress mac
);

// Broadcast an ARP request for an IPv4 address.
bool request(Network::IPv4Address targetIP);

}
