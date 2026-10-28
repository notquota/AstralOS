
#pragma once

#include "network.h"

namespace Astral::DHCP {

enum class State : uint8_t {
    Idle,
    Discovering,
    Requesting,
    Bound,
    Failed
};

struct Lease {
    Network::IPv4Address address;
    Network::IPv4Address subnetMask;
    Network::IPv4Address gateway;
    Network::IPv4Address dnsServer;
    Network::IPv4Address server;

    uint32_t leaseSeconds;
};

bool initialize();
bool start(uint32_t transactionId);

// Call once per elapsed second.
void tick();

State state();
bool configured();

const Lease& lease();

}
