
#include "session_controller.h"

#include "session.h"
#include "../usr/user_session.h"
#include "../security/lock.h"
#include "../input/input_manager.h"
#include "../services/clipboard.h"
#include "../tmp/temp_manager.h"

namespace Astral::Desktop::SessionController {

namespace {

State current = State::SignedOut;

void synchronize_input() {
    InputManager::set_locked(
        current != State::Active
    );
}

void clear_user_data() {
    Clipboard::clear();
    TempManager::clear();
}

}

void initialize() {
    Session::initialize();
    UserSession::initialize();
    Lock::initialize();

    Clipboard::initialize();
    TempManager::initialize();

    current = State::SignedOut;
    synchronize_input();
}

bool start_authenticated(
    uint32_t userId,
    const char* username
) {
    if (current != State::SignedOut)
        return false;

    // Both state modules require an already
    // authenticated caller.
    if (!UserSession::start_authenticated(
        userId, username
    ))
        return false;

    if (!Session::begin_authenticated_user()) {
        UserSession::end();
        return false;
    }

    current = State::Active;
    synchronize_input();

    return true;
}

bool lock_desktop() {
    if (current != State::Active)
        return false;

    if (!Lock::lock_desktop())
        return false;

    UserSession::lock();

    current = State::DesktopLocked;
    synchronize_input();

    return true;
}

bool lock_main() {
    if (current != State::Active &&
        current != State::DesktopLocked)
        return false;

    if (!Lock::lock_main())
        return false;

    UserSession::lock();

    current = State::MainLocked;
    synchronize_input();

    return true;
}

bool unlock(const char* password) {
    if (current != State::DesktopLocked &&
        current != State::MainLocked)
        return false;

    if (!Lock::unlock(password))
        return false;

    UserSession::unlock_authenticated();

    current = State::Active;
    synchronize_input();

    return true;
}

void sign_out() {
    clear_user_data();

    UserSession::end();
    Session::end();

    current = State::SignedOut;
    synchronize_input();
}

State state() {
    return current;
}

bool can_run_apps() {
    return current == State::Active &&
           Session::user_access_allowed() &&
           UserSession::active() &&
           !Lock::locked();
}

bool is_locked() {
    return current == State::DesktopLocked ||
           current == State::MainLocked ||
           Lock::locked();
}

}
