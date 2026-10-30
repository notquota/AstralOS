#include "desktop.h"

#include "core/desktop_manager.h"
#include "core/events.h"
#include "core/session_controller.h"

#include "taskbar/system_status.h"

#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../arch/x86_64/lowlevel.h"

namespace Astral::Desktop {

namespace {

uint32_t displayWidth = 0;
uint32_t displayHeight = 0;

bool desktopActive = false;

void send_mouse_event(
    Events::Type type,
    int32_t x,
    int32_t y,
    int32_t value = 0
) {
    Events::Event event{};

    event.type = type;
    event.x = x;
    event.y = y;
    event.value = value;

    DesktopManager::dispatch(event);
}

void send_keyboard_event(char character) {
    Events::Event event{};

    event.type = Events::Type::KeyDown;
    event.value =
        static_cast<unsigned char>(character);

    DesktopManager::dispatch(event);
}

void copy_time(
    char* target,
    const char* source
) {
    uint32_t i = 0;

    for (; i < 15 && source[i]; ++i)
        target[i] = source[i];

    target[i] = '\0';
}

bool same_time(
    const char* a,
    const char* b
) {
    for (uint32_t i = 0; i < 16; ++i) {
        if (a[i] != b[i])
            return false;

        if (a[i] == '\0')
            return true;
    }

    return true;
}

} // namespace

void set_display_size(
    uint32_t width,
    uint32_t height
) {
    displayWidth = width;
    displayHeight = height;
}

bool running() {
    return desktopActive;
}

void run() {
    if (desktopActive)
        return;

    if (displayWidth == 0 ||
        displayHeight == 0)
        return;

    // The Desktop is not a free sample
    // from the kernel privilege department.
    if (!SessionController::can_run_apps())
        return;

    DesktopManager::Configuration config{};

    config.screenWidth = displayWidth;
    config.screenHeight = displayHeight;

    if (!DesktopManager::initialize(config))
        return;

    if (!DesktopManager::start())
        return;

    desktopActive = true;

    Mouse::set_ui_capture(true);

    uint64_t previousMouseRevision =
        Mouse::revision();

    int32_t previousMouseX = Mouse::x();
    int32_t previousMouseY = Mouse::y();

    bool previousLeftDown =
        Mouse::left_down();

    // Empty old click records.
    // The past has no jurisdiction here.
    Mouse::take_left_click();

    SystemStatus::refresh();

    char lastTime[16]{};

    copy_time(
        lastTime,
        SystemStatus::time_text()
    );

    DesktopManager::invalidate();
    DesktopManager::render();

    // The Desktop gets its own little universe.
    // It is legally distinct from Microsoft Windows.
    while (
        DesktopManager::status() !=
        DesktopManager::Status::Stopped
    ) {
        bool activity = false;

        // IRQ12 is disabled in our mouse driver.
        // We now collect the mouse packets ourselves.
        //
        // Turns out a mouse cannot move if
        // nobody reads what the mouse is saying.
        Mouse::poll();

        const uint64_t revision =
            Mouse::revision();

        const int32_t currentX = Mouse::x();
        const int32_t currentY = Mouse::y();

        const bool leftDown =
            Mouse::left_down();

        if (revision != previousMouseRevision ||
            currentX != previousMouseX ||
            currentY != previousMouseY) {

            previousMouseRevision = revision;

            if (currentX != previousMouseX ||
                currentY != previousMouseY) {

                send_mouse_event(
                    Events::Type::MouseMove,
                    currentX,
                    currentY
                );

                previousMouseX = currentX;
                previousMouseY = currentY;
            }

            activity = true;
        }

        // Button transitioned from UP to DOWN.
        //
        // The window has officially been grabbed.
        if (leftDown && !previousLeftDown) {
            send_mouse_event(
                Events::Type::MouseDown,
                currentX,
                currentY
            );

            activity = true;
        }

        // Button transitioned from DOWN to UP.
        //
        // The hostage window has been released.
        if (!leftDown && previousLeftDown) {
            send_mouse_event(
                Events::Type::MouseUp,
                currentX,
                currentY
            );

            activity = true;
        }

        previousLeftDown = leftDown;

        // Clear compatibility click record so the
        // old API doesn't build up fictional clicks.
        Mouse::take_left_click();

        const int32_t wheel =
            Mouse::take_scroll();

        if (wheel != 0) {
            send_mouse_event(
                Events::Type::MouseWheel,
                currentX,
                currentY,
                wheel
            );

            activity = true;
        }

        const char character =
            Keyboard::read_char();

        if (character != '\0') {
            send_keyboard_event(character);
            activity = true;
        }

        // The RTC clocks in.
        // Unlike the browser engine.
        SystemStatus::refresh();

        const char* now =
            SystemStatus::time_text();

        if (!same_time(lastTime, now)) {
            copy_time(lastTime, now);
            DesktopManager::invalidate();
        }

        DesktopManager::pump();
        DesktopManager::render();

        if (!activity) {
            // A single pause instruction.
            // The CPU gets a microscopic vacation.
            astral_pause();
        }
    }

    Mouse::set_ui_capture(false);

    desktopActive = false;
}

} // namespace Astral::Desktop