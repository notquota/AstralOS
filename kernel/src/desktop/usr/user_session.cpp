
#include "user_session.h"

namespace Astral::Desktop::UserSession {

namespace {

Info current{};
uint64_t nextSessionId = 1;

bool copy_username(char* out, const char* text) {
    if (!text || !text[0])
        return false;

    uint32_t i = 0;

    while (text[i] && i + 1 < USERNAME_MAX) {
        out[i] = text[i];
        ++i;
    }

    if (text[i])
        return false;

    out[i] = '\0';
    return true;
}

void erase_username() {
    volatile char* p = current.username;
    for (uint32_t i = 0; i < USERNAME_MAX; ++i)
        p[i] = '\0';
}

}

void initialize() {
    current = {};
    current.state = State::SignedOut;
}

bool start_authenticated(
    uint32_t userId,
    const char* username
) {
    if (current.state != State::SignedOut)
        return false;

    char validated[USERNAME_MAX]{};

    if (!copy_username(validated, username))
        return false;

    current.userId = userId;

    for (uint32_t i = 0; i < USERNAME_MAX; ++i)
        current.username[i] = validated[i];

    current.sessionId = nextSessionId++;
    current.state = State::Active;

    return true;
}

void lock() {
    if (current.state == State::Active)
        current.state = State::Locked;
}

void unlock_authenticated() {
    // Called only after Security::authenticate_root()
    // or a future per-user authenticator succeeds.
    if (current.state == State::Locked)
        current.state = State::Active;
}

void end() {
    erase_username();
    current = {};
    current.state = State::SignedOut;
}

bool active() {
    return current.state == State::Active;
}

bool locked() {
    return current.state == State::Locked;
}

const Info& info() {
    return current;
}

}
