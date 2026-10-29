
#include "input_manager.h"

#include "../windows/window_manager.h"
#include "../windows/decorations.h"
#include "../windows/layout.h"

namespace Astral::Desktop::InputManager {

namespace {

bool screenLocked = false;

bool mouseDown = false;
bool resizing = false;

Windows::WindowId captured =
    Windows::INVALID_WINDOW;

Windows::WindowId hovered =
    Windows::INVALID_WINDOW;

Windows::Rect resizeStart{};
int32_t resizeMouseX = 0;
int32_t resizeMouseY = 0;

Action redraw() {
    return {ActionType::Redraw};
}

Action forward(
    ActionType type,
    Windows::WindowId window,
    const Events::Event& event
) {
    return {
        type,
        window,
        event.x,
        event.y,
        event.value,
        event.modifiers
    };
}

bool update_resize(int32_t x, int32_t y) {
    if (!resizing ||
        captured == Windows::INVALID_WINDOW)
        return false;

    const int64_t desiredW =
        static_cast<int64_t>(resizeStart.width) +
        (static_cast<int64_t>(x) - resizeMouseX);

    const int64_t desiredH =
        static_cast<int64_t>(resizeStart.height) +
        (static_cast<int64_t>(y) - resizeMouseY);

    Windows::Rect desired = resizeStart;

    if (desiredW < 240)
        desired.width = 240;
    else if (desiredW > INT32_MAX)
        desired.width = INT32_MAX;
    else
        desired.width = static_cast<int32_t>(desiredW);

    if (desiredH < 160)
        desired.height = 160;
    else if (desiredH > INT32_MAX)
        desired.height = INT32_MAX;
    else
        desired.height = static_cast<int32_t>(desiredH);

    const Windows::Rect limited =
        Layout::constrain(desired);

    return Windows::resize(
        captured,
        limited.width,
        limited.height
    );
}

Action handle_shortcut(Shortcuts::Action action) {
    const Windows::WindowId focused = Windows::focused();

    switch (action) {
        case Shortcuts::Action::ToggleLauncher:
            return {ActionType::ToggleLauncher};

        case Shortcuts::Action::LockDesktop:
            cancel_interaction();
            return {ActionType::LockDesktop};

        case Shortcuts::Action::OpenTerminal:
            return {ActionType::LaunchApp, 3};

        case Shortcuts::Action::OpenSettings:
            return {ActionType::LaunchApp, 2};

        case Shortcuts::Action::ShowDesktop:
            return {ActionType::ShowDesktop};

        case Shortcuts::Action::CloseWindow:
            if (focused != Windows::INVALID_WINDOW &&
                Windows::close(focused))
                return redraw();
            break;

        case Shortcuts::Action::MinimizeWindow:
            if (focused != Windows::INVALID_WINDOW &&
                Windows::minimize(focused))
                return redraw();
            break;

        case Shortcuts::Action::MaximizeWindow:
            if (focused != Windows::INVALID_WINDOW) {
                const Windows::Window* window =
                    Windows::get(focused);

                if (window) {
                    const bool maximized =
                        window->state ==
                        Windows::WindowState::Maximized;

                    const bool success =
                        maximized
                            ? Windows::restore(focused)
                            : Windows::maximize(focused);

                    if (success)
                        return redraw();
                }
            }
            break;

        case Shortcuts::Action::SwitchWindow: {
            // Find the topmost visible window that is
            // not the currently focused window.
            Windows::WindowId next =
                Windows::INVALID_WINDOW;

            uint32_t bestZ = 0;

            for (uint32_t i = 0;
                 i < Windows::count();
                 ++i) {
                const Windows::Window* window =
                    Windows::at(i);

                if (!window ||
                    !window->visible ||
                    window->id == focused ||
                    window->state ==
                        Windows::WindowState::Minimized)
                    continue;

                if (next == Windows::INVALID_WINDOW ||
                    window->zOrder > bestZ) {
                    next = window->id;
                    bestZ = window->zOrder;
                }
            }

            if (next != Windows::INVALID_WINDOW &&
                Windows::focus(next))
                return redraw();

            break;
        }

        default:
            break;
    }

    return {};
}

Action handle_mouse_down(const Events::Event& event) {
    mouseDown = true;

    captured = Windows::hit_test(event.x, event.y);

    if (captured == Windows::INVALID_WINDOW)
        return {};

    // Get the hit region before making any changes.
    Windows::Window* window = Windows::get(captured);

    if (!window) {
        captured = Windows::INVALID_WINDOW;
        return {};
    }

    const Decorations::Hit hit =
        Decorations::hit_test(
            *window, event.x, event.y
        );

    Windows::focus(captured);

    switch (hit) {
        case Decorations::Hit::Close:
            Windows::close(captured);
            cancel_interaction();
            return redraw();

        case Decorations::Hit::Minimize:
            Windows::minimize(captured);
            cancel_interaction();
            return redraw();

        case Decorations::Hit::Maximize: {
            const bool maximized =
                window->state ==
                Windows::WindowState::Maximized;

            if (maximized)
                Windows::restore(captured);
            else
                Windows::maximize(captured);

            cancel_interaction();
            return redraw();
        }

        case Decorations::Hit::TitleBar:
            Windows::begin_drag(
                captured, event.x, event.y
            );
            return redraw();

        case Decorations::Hit::ResizeBottomRight:
            resizeStart = window->bounds;
            resizeMouseX = event.x;
            resizeMouseY = event.y;
            resizing = true;
            return redraw();

        case Decorations::Hit::Client:
            return forward(
                ActionType::ForwardMouse,
                captured,
                event
            );

        default:
            cancel_interaction();
            return {};
    }
}

Action handle_mouse_move(const Events::Event& event) {
    hovered = Windows::hit_test(event.x, event.y);

    if (mouseDown && Windows::dragging()) {
        if (Windows::drag_to(event.x, event.y))
            return redraw();
    }

    if (mouseDown && resizing) {
        if (update_resize(event.x, event.y))
            return redraw();
    }

    if (hovered != Windows::INVALID_WINDOW)
        return forward(
            ActionType::ForwardMouse,
            hovered,
            event
        );

    return {};
}

}

void initialize() {
    screenLocked = false;
    mouseDown = false;
    resizing = false;
    captured = Windows::INVALID_WINDOW;
    hovered = Windows::INVALID_WINDOW;
    resizeStart = {};
}

void cancel_interaction() {
    Windows::end_drag();

    mouseDown = false;
    resizing = false;
    captured = Windows::INVALID_WINDOW;
    resizeStart = {};
}

void set_locked(bool value) {
    screenLocked = value;

    if (value)
        cancel_interaction();
}

bool locked() {
    return screenLocked;
}

Windows::WindowId hovered_window() {
    return hovered;
}

Windows::WindowId captured_window() {
    return captured;
}

Action dispatch(const Events::Event& event) {
    // Security policy: do not dispatch application
    // or window interactions when the session is locked.
    //
    // The lock-screen controller receives keyboard
    // input through its own restricted route.
    if (screenLocked)
        return {};

    switch (event.type) {
        case Events::Type::MouseDown:
            return handle_mouse_down(event);

        case Events::Type::MouseMove:
            return handle_mouse_move(event);

        case Events::Type::MouseUp: {
            const Windows::WindowId target = captured;
            const bool forwardToApp =
                mouseDown &&
                !resizing &&
                !Windows::dragging() &&
                target != Windows::INVALID_WINDOW;

            cancel_interaction();

            if (forwardToApp)
                return forward(
                    ActionType::ForwardMouse,
                    target,
                    event
                );

            return redraw();
        }

        case Events::Type::MouseWheel: {
            const Windows::WindowId target =
                Windows::hit_test(event.x, event.y);

            if (target != Windows::INVALID_WINDOW)
                return forward(
                    ActionType::ForwardScroll,
                    target,
                    event
                );

            return {};
        }

        case Events::Type::KeyDown: {
            const Shortcuts::KeyEvent key{
                static_cast<uint32_t>(event.value),
                event.modifiers,
                true
            };

            const Shortcuts::Action shortcut =
                Shortcuts::resolve(key);

            if (shortcut != Shortcuts::Action::None)
                return handle_shortcut(shortcut);

            const Windows::WindowId focused =
                Windows::focused();

            if (focused != Windows::INVALID_WINDOW)
                return forward(
                    ActionType::ForwardKey,
                    focused,
                    event
                );

            return {};
        }

        case Events::Type::LockScreen:
            cancel_interaction();
            return {ActionType::LockDesktop};

        case Events::Type::LaunchApp:
            return {
                ActionType::LaunchApp,
                event.target
            };

        case Events::Type::CloseWindow:
            if (Windows::close(event.target))
                return redraw();
            return {};

        case Events::Type::FocusWindow:
            if (Windows::focus(event.target))
                return redraw();
            return {};

        default:
            return {};
    }
}

}
