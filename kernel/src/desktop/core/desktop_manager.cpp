#include "desktop_manager.h"

#include "app_manager.h"
#include "session_controller.h"

#include "../graphics/image.h"
#include "../graphics/renderer.h"
#include "../graphics/wallpaper.h"
#include "../graphics/asset_bindings.h"

#include "../../../assets/desktop/generated/desktop_assets.h"

#include "../windows/window_manager.h"
#include "../windows/layout.h"
#include "../windows/decorations.h"

#include "../input/input_manager.h"

#include "../launcher/launcher.h"
#include "../launcher/app_registry.h"

#include "../taskbar/taskbar.h"
#include "../taskbar/system_status.h"
#include "../taskbar/top_status_bar.h"

#include "../widgets/notification_center.h"
#include "../widgets/desktop_tiles.h"

#include "../services/notifications.h"

#include "../security/lock.h"
#include "../security/lock_screen.h"

#include "../settings/registry.h"

namespace Astral::Desktop::DesktopManager {

namespace {

Status currentStatus = Status::Stopped;
Configuration screen{};

bool initialized = false;
bool dirty = false;

bool overlayPress = false;

// This prevents a window click from magically
// becoming a desktop cube click upon release.
//
// Previously, the mouse had diplomatic immunity.
bool tilePress = false;
uint32_t pressedApp = 0;

int32_t pressX = 0;
int32_t pressY = 0;

int32_t mouseX = 0;
int32_t mouseY = 0;

bool inside(
    int32_t mx,
    int32_t my,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height
) {
    return mx >= x &&
           my >= y &&
           static_cast<int64_t>(mx) <
               static_cast<int64_t>(x) + width &&
           static_cast<int64_t>(my) <
               static_cast<int64_t>(y) + height;
}

int32_t screen_width() {
    return static_cast<int32_t>(
        screen.screenWidth
    );
}

int32_t screen_height() {
    return static_cast<int32_t>(
        screen.screenHeight
    );
}

bool in_taskbar(int32_t y) {
    return y >= screen_height() - Taskbar::HEIGHT &&
           y < screen_height();
}

bool in_top_bar(int32_t y) {
    return y >= 0 && y < TopStatusBar::HEIGHT;
}

bool in_launcher(int32_t x, int32_t y) {
    return Launcher::visible() &&
           inside(
               x,
               y,
               12,
               screen_height() -
                   Taskbar::HEIGHT - 365 - 9,
               330,
               365
           );
}

bool in_notifications(int32_t x, int32_t y) {
    return NotificationCenter::visible() &&
           inside(
               x,
               y,
               screen_width() - 355 - 14,
               screen_height() -
                   Taskbar::HEIGHT - 390 - 10,
               355,
               390
           );
}

bool overlay_at(int32_t x, int32_t y) {
    return in_top_bar(y) ||
           in_taskbar(y) ||
           in_launcher(x, y) ||
           in_notifications(x, y);
}

void cancel_tile_press() {
    tilePress = false;
    pressedApp = 0;
}

void install_desktop_assets() {
    AssetBindings::Assets assets{};

    assets.logo =
        &Astral::Desktop::Assets::branding_astralos;

    assets.cursor =
        &Astral::Desktop::Assets::cursors_default;

    assets.files =
        &Astral::Desktop::Assets::apps_files;

    assets.settings =
        &Astral::Desktop::Assets::apps_settings;

    assets.terminal =
        &Astral::Desktop::Assets::apps_terminal;

    assets.monitor =
        &Astral::Desktop::Assets::apps_system_monitor;

    assets.about =
        &Astral::Desktop::Assets::apps_about;

    // PNGs reporting for duty.
    // Please do not initialize the renderer again
    // or all of their employment contracts vanish.
    AssetBindings::install(assets);
}

void synchronize_lock() {
    const bool locked =
        SessionController::is_locked();

    if (locked) {
        if (currentStatus != Status::Locked) {
            currentStatus = Status::Locked;

            InputManager::set_locked(true);

            Launcher::close();
            NotificationCenter::close();

            overlayPress = false;
            cancel_tile_press();

            dirty = true;
        }
    } else if (currentStatus == Status::Locked) {
        currentStatus = Status::Running;

        InputManager::set_locked(false);
        dirty = true;
    }
}

void show_desktop() {
    for (uint32_t i = 0;
         i < Windows::count();
         ++i) {

        const Windows::Window* window =
            Windows::at(i);

        if (!window)
            continue;

        if (window->state !=
            Windows::WindowState::Minimized) {
            Windows::minimize(window->id);
        }
    }

    dirty = true;
}

void handle_taskbar_action(
    const Taskbar::Action& action
) {
    switch (action.type) {
        case Taskbar::ActionType::ToggleLauncher:
            NotificationCenter::close();
            Launcher::toggle();
            dirty = true;
            return;

        case Taskbar::ActionType::LaunchApp:
            launch_app(action.target);
            return;

        case Taskbar::ActionType::FocusWindow:
            Windows::focus(action.target);
            dirty = true;
            return;

        case Taskbar::ActionType::RestoreWindow:
            Windows::restore(action.target);
            dirty = true;
            return;

        case Taskbar::ActionType::MinimizeWindow:
            Windows::minimize(action.target);
            dirty = true;
            return;

        case Taskbar::ActionType::OpenQuickSettings:
            Launcher::close();
            NotificationCenter::toggle();
            dirty = true;
            return;

        default:
            return;
    }
}

void handle_overlay_click(
    const Events::Event& event
) {
    const int32_t x = event.x;
    const int32_t y = event.y;

    if (in_top_bar(y)) {
        // The tray isn't interactive yet.
        // It's still in the awkward internship phase.
        return;
    }

    if (in_taskbar(y)) {
        handle_taskbar_action(
            Taskbar::click(
                x,
                y,
                screen_width(),
                screen_height()
            )
        );

        return;
    }

    if (NotificationCenter::visible()) {
        const bool wasInside =
            in_notifications(x, y);

        NotificationCenter::click(
            x,
            y,
            screen_width(),
            screen_height()
        );

        dirty = true;

        if (wasInside)
            return;
    }

    if (Launcher::visible()) {
        const bool wasInside =
            in_launcher(x, y);

        const Launcher::Result result =
            Launcher::click(
                x,
                y,
                screen_width(),
                screen_height()
            );

        dirty = true;

        if (result.selected)
            launch_app(result.appId);

        if (wasInside)
            return;
    }
}

void handle_input_action(
    const InputManager::Action& action,
    const Events::Event& event
) {
    switch (action.type) {
        case InputManager::ActionType::Redraw:
            AppManager::reconcile();
            dirty = true;
            break;

        case InputManager::ActionType::LaunchApp:
            launch_app(action.target);
            break;

        case InputManager::ActionType::ToggleLauncher:
            Launcher::toggle();
            NotificationCenter::close();
            dirty = true;
            break;

        case InputManager::ActionType::LockDesktop:
            if (SessionController::lock_desktop())
                synchronize_lock();
            break;

        case InputManager::ActionType::ShowDesktop:
            show_desktop();
            break;

        case InputManager::ActionType::ForwardKey:
        case InputManager::ActionType::ForwardMouse:
        case InputManager::ActionType::ForwardScroll:
            if (AppManager::dispatch(action, event))
                dirty = true;

            synchronize_lock();
            break;

        default:
            break;
    }
}

} // namespace

bool initialize(const Configuration& config) {
    if (config.screenWidth < 800 ||
        config.screenHeight < 600)
        return false;

    if (!Image::begin(
        config.screenWidth,
        config.screenHeight
    ))
        return false;

    screen = config;

    Events::initialize();

    Windows::initialize(
        screen.screenWidth,
        screen.screenHeight,
        Taskbar::HEIGHT
    );

    Layout::initialize({
        screen_width(),
        screen_height(),
        Taskbar::HEIGHT
    });

    AppRegistry::initialize();
    AppManager::initialize();
    InputManager::initialize();

    Renderer::initialize();
    Wallpaper::initialize();

    Taskbar::initialize();
    Launcher::initialize();
    Decorations::initialize();

    NotificationCenter::initialize();

    Notifications::initialize();
    SystemStatus::initialize();

    TopStatusBar::initialize();

    Settings::initialize();

    Widgets::DesktopTiles::initialize();

    Widgets::DesktopTiles::set_screen_size(
        screen_width(),
        screen_height()
    );

    install_desktop_assets();

    currentStatus = Status::Stopped;

    overlayPress = false;
    cancel_tile_press();

    mouseX = 0;
    mouseY = 0;

    initialized = true;
    dirty = true;

    return true;
}

bool start() {
    if (!initialized ||
        currentStatus != Status::Stopped)
        return false;

    if (!SessionController::can_run_apps())
        return false;

    currentStatus = Status::Running;

    InputManager::set_locked(false);

    overlayPress = false;
    cancel_tile_press();

    dirty = true;

    return true;
}

void stop() {
    InputManager::cancel_interaction();

    currentStatus = Status::Stopped;

    overlayPress = false;
    cancel_tile_press();

    dirty = false;
}

Status status() {
    return currentStatus;
}

void invalidate() {
    if (currentStatus != Status::Stopped)
        dirty = true;
}

bool launch_app(uint32_t appId) {
    if (currentStatus != Status::Running ||
        !SessionController::can_run_apps())
        return false;

    const Windows::WindowId id =
        AppManager::launch(appId);

    if (id == Windows::INVALID_WINDOW)
        return false;

    Launcher::close();
    dirty = true;

    return true;
}

void dispatch(const Events::Event& event) {
    if (currentStatus == Status::Stopped)
        return;

    synchronize_lock();

    if (currentStatus == Status::Locked) {
        if (event.type == Events::Type::KeyDown) {
            LockScreen::handle_key(
                static_cast<char>(event.value)
            );

            synchronize_lock();
            dirty = true;
        }

        return;
    }

    if (event.type == Events::Type::MouseMove) {
        mouseX = event.x;
        mouseY = event.y;

        // The cursor position is ALWAYS updated.
        // It no longer needs permission from
        // the dirty flag's HOA.
        Renderer::move_cursor(mouseX, mouseY);

        if (Widgets::DesktopTiles::hover(
            mouseX,
            mouseY
        )) {
            dirty = true;
        }

        if (tilePress) {
            const int64_t dx =
                static_cast<int64_t>(mouseX) - pressX;

            const int64_t dy =
                static_cast<int64_t>(mouseY) - pressY;

            // Dragging more than six pixels cancels
            // shortcut activation.
            //
            // No accidental Browser launches while
            // trying to rearrange the furniture.
            if (dx * dx + dy * dy > 36)
                cancel_tile_press();
        }
    }

    if (event.type == Events::Type::MouseDown) {
        cancel_tile_press();
        overlayPress = false;

        if (overlay_at(event.x, event.y)) {
            overlayPress = true;
            return;
        }

        // The window manager gets priority.
        // Clicking Settings must not secretly
        // open Files behind it.
        if (Windows::hit_test(
            event.x,
            event.y
        ) == Windows::INVALID_WINDOW) {

            const uint32_t app =
                Widgets::DesktopTiles::click(
                    event.x,
                    event.y
                );

            if (app != 0) {
                tilePress = true;
                pressedApp = app;
                pressX = event.x;
                pressY = event.y;
                return;
            }
        }
    }

    if (event.type == Events::Type::MouseUp) {
        if (overlayPress) {
            overlayPress = false;
            cancel_tile_press();

            handle_overlay_click(event);
            return;
        }

        if (tilePress) {
            const uint32_t startedOn =
                pressedApp;

            cancel_tile_press();

            if (!overlay_at(event.x, event.y) &&
                Widgets::DesktopTiles::click(
                    event.x,
                    event.y
                ) == startedOn) {

                launch_app(startedOn);
            }

            return;
        }
    }

    if (event.type == Events::Type::MouseWheel) {
        if (in_notifications(event.x, event.y)) {
            NotificationCenter::scroll(event.value);
            dirty = true;
            return;
        }

        if (in_launcher(event.x, event.y)) {
            Launcher::scroll(event.value);
            dirty = true;
            return;
        }
    }

    const InputManager::Action action =
        InputManager::dispatch(event);

    handle_input_action(action, event);
}

void pump() {
    if (currentStatus == Status::Stopped)
        return;

    Events::Event event{};

    while (Events::pop(event))
        dispatch(event);
}

void render() {
    if (currentStatus == Status::Stopped ||
        !dirty)
        return;

    synchronize_lock();

    if (currentStatus == Status::Locked) {
        LockScreen::render();
        dirty = false;
        return;
    }

    Renderer::compose();

    NotificationCenter::render(
        screen_width(),
        screen_height()
    );

    // The wallpaper, apps and notifications
    // are now a single coherent hallucination.
    Renderer::present();

    dirty = false;
}

} // namespace Astral::Desktop::DesktopManager