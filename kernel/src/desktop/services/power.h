
#pragma once
#include <stdint.h>

namespace Astral::Desktop::Power {

enum class Action : uint8_t {
    None,
    Shutdown,
    Restart,
    Sleep,
    Hibernate,
    LogOut,
    Lock
};

enum class Result : uint8_t {
    Accepted,
    Denied,
    Unsupported,
    Invalid
};

using AuthorizationCallback = bool (*)(Action);
using ExecutionCallback = bool (*)(Action);

void initialize();

void set_authorizer(AuthorizationCallback callback);
void set_executor(ExecutionCallback callback);

Result request(Action action);

Action pending();
void clear_pending();

}
