
#include "power.h"

namespace Astral::Desktop::Power {

namespace {

AuthorizationCallback authorization = nullptr;
ExecutionCallback execution = nullptr;
Action lastPending = Action::None;

}

void initialize() {
    authorization = nullptr;
    execution = nullptr;
    lastPending = Action::None;
}

void set_authorizer(AuthorizationCallback callback) {
    authorization = callback;
}

void set_executor(ExecutionCallback callback) {
    execution = callback;
}

Result request(Action action) {
    if (action == Action::None)
        return Result::Invalid;

    if (!authorization ||
        !authorization(action))
        return Result::Denied;

    if (!execution)
        return Result::Unsupported;

    if (!execution(action))
        return Result::Unsupported;

    lastPending = action;
    return Result::Accepted;
}

Action pending() {
    return lastPending;
}

void clear_pending() {
    lastPending = Action::None;
}

}
