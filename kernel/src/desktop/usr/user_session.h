
#pragma once
#include <stdint.h>

namespace Astral::Desktop::UserSession {

constexpr uint32_t USERNAME_MAX = 48;

enum class State : uint8_t {
    SignedOut,
    Active,
    Locked
};

struct Info {
    uint32_t userId;
    char username[USERNAME_MAX];
    State state;
    uint64_t sessionId;
};

void initialize();

// Only the authenticated login controller may call this.
// This function does not check a password.
bool start_authenticated(uint32_t userId, const char* username);

void lock();
void unlock_authenticated();
void end();

bool active();
bool locked();

const Info& info();

}
