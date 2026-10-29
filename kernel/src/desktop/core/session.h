
#pragma once
#include <stdint.h>

namespace Astral::Desktop::Session {

enum class Permission : uint8_t {
    None,
    User
};

struct State {
    bool active;
    bool authenticated;
    Permission permission;
    uint64_t revision;
};

void initialize();
bool begin_authenticated_user();
void end();

bool active();
bool authenticated();
bool user_access_allowed();

const State& state();

}
