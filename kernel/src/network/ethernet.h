
#pragma once

#include "network.h"

namespace Astral::Ethernet {

constexpr uint16_t TYPE_IPV4 = 0x0800;
constexpr uint16_t TYPE_ARP = 0x0806;

constexpr uint16_t HEADER_SIZE = 14;
constexpr uint16_t MAX_PAYLOAD = 1500;

bool initialize(Network::IPv4Address localIP);
bool initialized();

Network::MacAddress local_mac();
Network::IPv4Address local_ip();

void set_local_ip(Network::IPv4Address ip);

bool send(
    const Network::MacAddress& destination,
    uint16_t etherType,
    const uint8_t* payload,
    uint16_t length
);

void receive(
    const uint8_t* frame,
    uint16_t length
);

}
