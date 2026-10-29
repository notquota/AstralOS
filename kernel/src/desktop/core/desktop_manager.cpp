
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

#include "../widgets/notification_center.h"
#include "../widgets/desktop_tiles.h"

#include "../services/notifications.h"

#include "../security/lock.h"
#include "../security/lock_screen.h"

#include "../settings/registry.h"

#include "../apps/browser/browser_app.h"

namespace Astral::Desktop::DesktopManager {

namespace {

// These variables know everything.
// Unfortunately, none of them are willing to testify.
Status currentStatus = Status::Stopped;
Configuration screen{};

bool initialized = false;
bool dirty = false;

// Two booleans guarding the mouse.
// Security budget: approximately seven cents.
bool overlayPress = false;
bool suppressMouseUp = false;

int32_t mouseX = 0;
int32_t mouseY = 0;

// This function determines whether a point is inside a box.
// Humanity has reached the peak of technological innovation.
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

// We asked the screen how wide it was.
// It answered. Suspiciously quickly.
int32_t screen_width() {
    return static_cast<int32_t>(
        screen.screenWidth
    );
}

// Height is just width with vertical ambitions.
int32_t screen_height() {
    return static_cast<int32_t>(
        screen.screenHeight
    );
}

// The taskbar has claimed the bottom of the screen.
// The rest of the pixels are filing a complaint.
bool in_taskbar(int32_t y) {
    return y >= screen_height() - Taskbar::HEIGHT &&
           y < screen_height();
}

// The launcher exists in a rectangle.
// Very brave of it.
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

// Notifications live in the corner because
// nobody invited them to the center of attention.
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

// This is where clicks go when they think
// they're too important for regular windows.
bool overlay_at(int32_t x, int32_t y) {
    return in_taskbar(y) ||
           in_launcher(x, y) ||
           in_notifications(x, y);
}

// PNG DEPLOYMENT OPERATION.
//
// We spent all this time making icons.
// It would be extremely embarrassing if
// the operating system simply refused to acknowledge them.
//
// That has happened before.
// We do not discuss the incident.
void install_desktop_assets() {
    AssetBindings::Assets assets{};

    // Corporate branding.
    // The cube has a legal department now.
    assets.logo =
        &Astral::Desktop::Assets::branding_astralos;

    // Our mouse cursor has finally received
    // its government-issued identification.
    assets.cursor =
        &Astral::Desktop::Assets::cursors_default;

    // The file manager.
    // It manages approximately three files
    // and fourteen existential questions.
    assets.files =
        &Astral::Desktop::Assets::apps_files;

    // Settings: 120 ways to change something
    // and 120 opportunities to break it.
    assets.settings =
        &Astral::Desktop::Assets::apps_settings;

    // A terminal inside an operating system
    // that already has a terminal.
    //
    // We're putting a terminal in your terminal
    // so you can terminal while you terminal.
    assets.terminal =
        &Astral::Desktop::Assets::apps_terminal;

    // System Monitor.
    // Watches the CPU suffer in real time.
    assets.monitor =
        &Astral::Desktop::Assets::apps_system_monitor;

    // About AstralOS.
    // In case somebody boots the ISO and
    // genuinely has no idea what they've done.
    assets.about =
        &Astral::Desktop::Assets::apps_about;

    // Release the PNGs into their natural habitat.
    //
    // This happens AFTER initialization because
    // initializing the UI afterward would reset
    // everything like an emotionally unstable router.
    AssetBindings::install(assets);
}

// LOCK SCREEN MANAGEMENT.
//
// A sophisticated system for telling the user:
// "Yes, this is your computer."
// "No, you may not touch it right now."
void synchronize_lock() {
    const bool locked =
        SessionController::is_locked();

    if (locked) {
        if (currentStatus != Status::Locked) {
            currentStatus = Status::Locked;

            // No keyboard adventures behind the lock.
            InputManager::set_locked(true);

            // Close the floating UI before it tries
            // to escape the security boundary.
            Launcher::close();
            NotificationCenter::close();

            overlayPress = false;
            suppressMouseUp = false;

            dirty = true;
        }
    } else if (currentStatus == Status::Locked) {
        // The password has passed judgment.
        // The computer has decided to forgive us.
        currentStatus = Status::Running;

        InputManager::set_locked(false);
        dirty = true;
    }
}

// Minimizes every window.
//
// The windows are not deleted.
// They are simply asked to reconsider
// their presence on the screen.
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

// Taskbar diplomacy.
//
// Every click submits a formal request.
// The manager decides whether the request
// deserves a window or a restraining order.
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
            // The taskbar has nothing to say.
            // Rare behavior.
            return;
    }
}

// Mouse clicks are basically paperwork.
//
// The taskbar, notification center and launcher
// all believe they're the main character.
void handle_overlay_click(
    const Events::Event& event
) {
    const int32_t x = event.x;
    const int32_t y = event.y;

    // First stop: the taskbar.
    // Management insists on priority boarding.
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

    // Notifications: tiny rectangles containing
    // information you probably didn't ask for.
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

    // The launcher has finally found
    // something productive to do.
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

// This function is the customer service desk
// for every input event in AstralOS.
//
// Satisfaction is not guaranteed.
// A refund is not available.
// There is no manager.
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
            // Please deliver this package
            // to the appropriate application.
            //
            // Do not leave it with the neighbors.
            if (AppManager::dispatch(action, event))
                dirty = true;

            synchronize_lock();
            break;

        default:
            // Input event declined.
            // Reason: unspecified cosmic interference.
            break;
    }
}

} // namespace

// ==================================================
// DESKTOP INITIALIZATION
// ==================================================
//
// We are about to initialize approximately
// seventeen independent systems.
//
// Surely nothing could possibly go wrong.
//
// Famous last words of every kernel developer.
bool initialize(const Configuration& config) {
    if (config.screenWidth < 800 ||
        config.screenHeight < 600)
        return false;

    // No, we cannot render a 9000K monitor
    // with a canvas designed for 1600x900.
    // The pixels have contractual limitations.
    if (!Image::begin(
        config.screenWidth,
        config.screenHeight
    ))
        return false;

    screen = config;

    Events::initialize();

    // Windows first.
    // Not the Microsoft kind.
    // We have enough problems already.
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

    // The graphics department has opened.
    // Nobody remembered to bring coffee.
    Renderer::initialize();
    Wallpaper::initialize();

    Taskbar::initialize();
    Launcher::initialize();
    Decorations::initialize();

    NotificationCenter::initialize();

    Notifications::initialize();
    SystemStatus::initialize();

    // 120 settings.
    // Some operating systems have fewer regrets.
    Settings::initialize();

    // The apps are now individual cubes.
    // The cube population is expanding.
    Widgets::DesktopTiles::initialize();

    Widgets::DesktopTiles::set_screen_size(
        screen_width(),
        screen_height()
    );

    // Finally, all those PNG files
    // can stop being expensive decoration.
    install_desktop_assets();

    // Signed-out users don't get a free GUI.
    // Security would like to keep its job.
    currentStatus = Status::Stopped;

    overlayPress = false;
    suppressMouseUp = false;

    mouseX = 0;
    mouseY = 0;

    initialized = true;
    dirty = true;

    return true;
}

// The ceremonial cutting of the ribbon.
//
// The ribbon is a boolean.
// We couldn't afford scissors.
bool start() {
    if (!initialized ||
        currentStatus != Status::Stopped)
        return false;

    if (!SessionController::can_run_apps())
        return false;

    currentStatus = Status::Running;

    InputManager::set_locked(false);

    overlayPress = false;
    suppressMouseUp = false;

    dirty = true;
    return true;
}

// Everybody out.
// The Desktop has entered its retirement era.
void stop() {
    InputManager::cancel_interaction();

    currentStatus = Status::Stopped;

    overlayPress = false;
    suppressMouseUp = false;

    dirty = false;
}

// The official government document
// confirming whether the Desktop is alive.
Status status() {
    return currentStatus;
}

// One pixel changes.
// We redraw the entire civilization.
void invalidate() {
    if (currentStatus != Status::Stopped)
        dirty = true;
}

// APPLICATION LAUNCHING.
//
// User clicks an icon.
// Kernel negotiates with sixteen subsystems.
// Window appears.
//
// Modern technology is beautiful.
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

// ==================================================
// EVENT DISPATCH
// ==================================================
//
// Every mouse movement, keyboard press and click
// must pass through this function.
//
// If something explodes, this is the crime scene.
void dispatch(const Events::Event& event) {
    if (currentStatus == Status::Stopped)
        return;

    synchronize_lock();

    // The lock screen gets exclusive custody
    // of keyboard input.
    //
    // Yes, we've tried asking nicely.
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

    // The mouse has moved.
    // A historic moment for computer science.
    if (event.type == Events::Type::MouseMove) {
        mouseX = event.x;
        mouseY = event.y;

        // The cubes are observing the cursor.
        // Their intentions remain classified.
        if (Widgets::DesktopTiles::hover(
            mouseX,
            mouseY
        )) {
            dirty = true;
        }

        // Don't draw directly over a frame
        // that is about to be replaced.
        // That's how we get cursed cursor trails.
        if (!dirty)
            Renderer::move_cursor(mouseX, mouseY);
    }

    // MouseDown enters the building.
    // Overlay security checks its credentials.
    if (event.type == Events::Type::MouseDown) {
        if (overlay_at(event.x, event.y)) {
            overlayPress = true;
            return;
        }

        overlayPress = false;
    }

    // MouseUp is where the actual drama happens.
    if (event.type == Events::Type::MouseUp) {
        if (overlayPress) {
            overlayPress = false;
            suppressMouseUp = false;

            handle_overlay_click(event);
            return;
        }

        if (suppressMouseUp) {
            suppressMouseUp = false;
            return;
        }

        // The cubes have been granted permission
        // to open actual applications.
        //
        // Previously they were just very expensive
        // PNGs enjoying unemployment.
        if (!overlay_at(event.x, event.y)) {
            const uint32_t appId =
                Widgets::DesktopTiles::click(
                    event.x,
                    event.y
                );

            if (appId != 0) {
                launch_app(appId);
                return;
            }
        }
    }

    // The scroll wheel has entered the chat.
    // Nobody can stop it now.
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

    // Anything not claimed by the taskbar
    // or the cubes gets delivered to windows.
    //
    // May the strongest widget survive.
    const InputManager::Action action =
        InputManager::dispatch(event);

    handle_input_action(action, event);
}

// Event processing.
//
// Essentially an assembly line,
// except the products are keyboard events
// and the workers are extremely stressed.
void pump() {
    if (currentStatus == Status::Stopped)
        return;

    Events::Event event{};

    while (Events::pop(event))
        dispatch(event);
}

// ==================================================
// FINAL RENDERING
// ==================================================
//
// Here we take an enormous pile of unrelated state
// and convince the framebuffer that it's a desktop.
//
// The framebuffer believes us.
// We have no idea why.
void render() {
    if (currentStatus == Status::Stopped || !dirty)
        return;

    synchronize_lock();

    if (currentStatus == Status::Locked) {
        LockScreen::render();

        dirty = false;
        return;
    }

    // The renderer assembles the wallpaper,
    // cubes, application windows and taskbar.
    //
    // Note: DesktopTiles::render() belongs inside
    // Renderer::compose(), AFTER the wallpaper
    // and BEFORE application windows.
    //
    // Otherwise the cubes will float over windows
    // like tiny, rectangular sleep paralysis demons.
    Renderer::compose();

    NotificationCenter::render(
        screen_width(),
        screen_height()
    );

    // Present the final result.
    // If it looks good, i planned it.
    // If it looks bad, it's the jews 100%.
    Renderer::present();

    dirty = false;
}

} // namespace Astral::Desktop::DesktopManager
