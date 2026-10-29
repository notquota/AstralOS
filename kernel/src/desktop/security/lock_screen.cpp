
#include "lock_screen.h"
#include "lock.h"

#include "../core/session_controller.h"
#include "../graphics/image.h"
#include "../taskbar/system_status.h"

#include <stdint.h>

namespace Astral::Desktop::LockScreen {

namespace {

char password[64]{};
uint32_t passwordLength = 0;
bool authenticationFailed = false;

void clear_password() {
    volatile char* data = password;

    for (uint32_t i = 0; i < sizeof(password); ++i)
        data[i] = '\0';

    passwordLength = 0;
}

}

bool render() {
    if (!SessionController::is_locked()) {
        clear_password();
        return false;
    }

    const int32_t w =
        static_cast<int32_t>(Image::width());

    const int32_t h =
        static_cast<int32_t>(Image::height());

    if (w <= 0 || h <= 0)
        return true;

    Image::clear(0x091222);

    const int32_t panelW = 440;
    const int32_t panelH = 278;

    const int32_t x = (w - panelW) / 2;
    const int32_t y = (h - panelH) / 2;

    Image::rectangle(
        x, y, panelW, panelH,
        0x17233B
    );

    Image::rectangle(
        x, y, panelW, 4,
        0xA78BFA
    );

    Image::text(
        x + 35, y + 35,
        "AstralOS",
        0xEDF1FF, 3
    );

    Image::text(
        x + 35, y + 89,
        SessionController::state() ==
                SessionController::State::MainLocked
            ? "Main session locked"
            : "Desktop locked",
        0xC7B7FF, 2
    );

    Image::text(
        x + 35, y + 132,
        "Enter account password",
        0xAAB9D3
    );

    Image::rectangle(
        x + 35, y + 158,
        370, 42,
        0x253553
    );

    const uint32_t visible =
        passwordLength > 32 ? 32 : passwordLength;

    for (uint32_t i = 0; i < visible; ++i) {
        Image::text(
            x + 49 + static_cast<int32_t>(i * 9),
            y + 175,
            "*",
            0xEDF1FF
        );
    }

    if (authenticationFailed) {
        Image::text(
            x + 35, y + 218,
            "Authentication failed",
            0xEF94A1
        );
    } else {
        Image::text(
            x + 35, y + 218,
            "Press Enter to unlock",
            0xAAB9D3
        );
    }

    Image::text(
        x + 35, y + 245,
        "User input remains locked",
        0x7F90B0
    );

    SystemStatus::refresh();

    Image::text(
        w - 105, 25,
        SystemStatus::time_text(),
        0xEEF2FF
    );

    Image::present();
    return true;
}

bool handle_key(char key) {
    if (!SessionController::is_locked()) {
        clear_password();
        authenticationFailed = false;
        return false;
    }

    if (key == 0)
        return true;

    if (key == '\b' || key == 127) {
        if (passwordLength > 0)
            password[--passwordLength] = '\0';

        authenticationFailed = false;
        return true;
    }

    if (key == '\r' || key == '\n') {
        password[passwordLength] = '\0';

        const bool success =
            SessionController::unlock(password);

        clear_password();
        authenticationFailed = !success;

        return true;
    }

    if (key >= 32 && key <= 126 &&
        passwordLength + 1 < sizeof(password)) {
        password[passwordLength++] = key;
        password[passwordLength] = '\0';
        authenticationFailed = false;
    }

    return true;
}

}
