
#include "desktop.h"

#include "core/desktop_manager.h"
#include "core/events.h"
#include "core/session_controller.h"

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

    // We can't render into a framebuffer
    // that apparently doesn't exist. Rude.
    if (displayWidth == 0 ||
        displayHeight == 0)
        return;

    // The user must already be authenticated.
    // No free HPT privileges with your wallpaper.
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

    // Mouse belongs to the GUI for now.
    Mouse::set_ui_capture(true);

    uint64_t previousMouseRevision =
        Mouse::revision();

    int32_t previousMouseX = Mouse::x();
    int32_t previousMouseY = Mouse::y();

    DesktopManager::invalidate();
    DesktopManager::render();

    // The new Desktop lives here.
    // The old drawing loop has been evicted.
    while (
        DesktopManager::status() !=
        DesktopManager::Status::Stopped
    ) {
        bool activity = false;

        const uint64_t revision =
            Mouse::revision();

        const int32_t currentX = Mouse::x();
        const int32_t currentY = Mouse::y();

        if (revision != previousMouseRevision ||
            currentX != previousMouseX ||
            currentY != previousMouseY) {

            previousMouseRevision = revision;
            previousMouseX = currentX;
            previousMouseY = currentY;

            send_mouse_event(
                Events::Type::MouseMove,
                currentX,
                currentY
            );

            activity = true;
        }

        if (Mouse::take_left_click()) {
            // Current driver exposes click events,
            // not a proper held/released mouse state.
            // So dragging still needs driver work.
            send_mouse_event(
                Events::Type::MouseDown,
                currentX,
                currentY
            );

            send_mouse_event(
                Events::Type::MouseUp,
                currentX,
                currentY
            );

            activity = true;
        }

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

        DesktopManager::pump();
        DesktopManager::render();

        if (!activity) {
            // No need to cook the CPU just because
            // somebody is admiring their wallpaper.
            astral_pause();
        }
    }

    Mouse::set_ui_capture(false);

    desktopActive = false;
}

} // namespace Astral::Desktop
