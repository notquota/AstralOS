
#pragma once

#include "network.h"

namespace Astral::DNS {

enum class State : uint8_t {
    Idle,
    Waiting,
    Resolved,
    Failed
};

bool initialize(Network::IPv4Address dnsServer);

bool resolve(
    const char* hostname,
    uint16_t queryId
);

// Call once per elapsed second.
void tick();

State state();

bool result(Network::IPv4Address& address);

}
