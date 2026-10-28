
#pragma once

#include "network.h"

namespace Astral::IPv4 {

constexpr uint8_t PROTOCOL_ICMP = 1;
constexpr uint8_t PROTOCOL_UDP = 17;

struct Route {
    Network::IPv4Address subnetMask;
    Network::IPv4Address gateway;
};

void configure(
    Network::IPv4Address subnetMask,
    Network::IPv4Address gateway
);

const Route& route();

bool send(
    Network::IPv4Address destination,
    uint8_t protocol,
    const uint8_t* payload,
    uint16_t length
);

void receive(
    const uint8_t* packet,
    uint16_t length,
    Network::MacAddress sourceMac
);

}
