#include "app_manager.h"
#include "session_controller.h"

#include "../launcher/app_registry.h"
#include "../windows/window_manager.h"
#include "../windows/decorations.h"

#include "../apps/terminal/terminal_app.h"
#include "../apps/settings/settings_app.h"
#include "../apps/files/file_manager.h"
#include "../apps/system_monitor/system_monitor.h"
#include "../apps/about/about_app.h"
#include "../apps/browser/browser_app.h"

namespace Astral::Desktop::AppManager {

namespace {

Instance instances[MAX_INSTANCES]{};
uint32_t instanceCount = 0;

int32_t nextX = 95;
int32_t nextY = 75;

Instance* find_instance(Windows::WindowId id) {
    for (uint32_t i = 0; i < instanceCount; ++i) {
        if (instances[i].windowId == id)
            return &instances[i];
    }

    return nullptr;
}

Instance* find_app(uint32_t appId) {
    for (uint32_t i = 0; i < instanceCount; ++i) {
        if (instances[i].appId == appId)
            return &instances[i];
    }

    return nullptr;
}

void initialize_app(uint32_t appId) {
    switch (appId) {
        case 1:
            Apps::FileManager::initialize();
            break;

        case 2:
            Apps::SettingsApp::initialize();
            break;

        case 3:
            Apps::TerminalApp::initialize();
            break;

        case 4:
            Apps::SystemMonitor::initialize();
            break;

        case 5:
            Apps::AboutApp::initialize();
            break;

        case 6:
            // The Browser finally has a birth certificate.
            Apps::BrowserApp::initialize();
            break;

        default:
            break;
    }
}

Windows::Rect default_bounds(uint32_t appId) {
    int32_t width = 620;
    int32_t height = 430;

    switch (appId) {
        case 2:
            width = 780;
            height = 560;
            break;

        case 3:
            width = 650;
            height = 400;
            break;

        case 6:
            width = 800;
            height = 560;
            break;

        case 5:
            width = 545;
            height = 420;
            break;

        default:
            break;
    }

    Windows::Rect bounds{
        nextX,
        nextY,
        width,
        height
    };

    nextX += 28;
    nextY += 25;

    if (nextX > 250)
        nextX = 95;

    if (nextY > 200)
        nextY = 75;

    return bounds;
}

} // namespace

void initialize() {
    for (auto& instance : instances)
        instance = {};

    instanceCount = 0;
    nextX = 95;
    nextY = 75;
}

Windows::WindowId launch(uint32_t appId) {
    if (!SessionController::can_run_apps())
        return Windows::INVALID_WINDOW;

    const AppRegistry::Application* app =
        AppRegistry::find(appId);

    if (!app || !app->available)
        return Windows::INVALID_WINDOW;

    // The apps are singletons for now.
    // Multiple copies would start a custody battle.
    Instance* existing = find_app(appId);

    if (existing) {
        Windows::Window* window =
            Windows::get(existing->windowId);

        if (window) {
            if (window->state ==
                Windows::WindowState::Minimized) {
                Windows::restore(existing->windowId);
            } else {
                Windows::focus(existing->windowId);
            }

            return existing->windowId;
        }

        reconcile();
    }

    if (instanceCount >= MAX_INSTANCES)
        return Windows::INVALID_WINDOW;

    const Windows::WindowId id =
        Windows::create(
            appId,
            app->name,
            default_bounds(appId)
        );

    if (id == Windows::INVALID_WINDOW)
        return id;

    initialize_app(appId);

    instances[instanceCount++] = {
        appId,
        id,
        true
    };

    return id;
}

bool close(Windows::WindowId windowId) {
    if (!find_instance(windowId))
        return false;

    if (!Windows::close(windowId))
        return false;

    reconcile();
    return true;
}

void reconcile() {
    for (uint32_t i = 0; i < instanceCount;) {
        if (Windows::get(instances[i].windowId)) {
            ++i;
            continue;
        }

        for (uint32_t j = i + 1;
             j < instanceCount; ++j) {
            instances[j - 1] = instances[j];
        }

        --instanceCount;
        instances[instanceCount] = {};
    }
}

bool dispatch(
    const InputManager::Action& action,
    const Events::Event& event
) {
    if (!SessionController::can_run_apps())
        return false;

    Instance* instance =
        find_instance(action.target);

    if (!instance)
        return false;

    const Windows::Window* window =
        Windows::get(instance->windowId);

    if (!window ||
        !window->visible ||
        window->state ==
            Windows::WindowState::Minimized)
        return false;

    // Keyboard input belongs to the focused app.
    // Not whichever window screams loudest.
    if (action.type ==
        InputManager::ActionType::ForwardKey) {
        if (event.type != Events::Type::KeyDown ||
            Windows::focused() != instance->windowId)
            return false;

        if (instance->appId == 6)
            return Apps::BrowserApp::handle_key(
                static_cast<char>(event.value)
            );

        if (instance->appId == 3) {
            Apps::TerminalApp::handle_key(
                static_cast<char>(event.value)
            );

            return true;
        }

        return false;
    }

    if (action.type ==
        InputManager::ActionType::ForwardScroll) {
        switch (instance->appId) {
            case 1:
                Apps::FileManager::scroll(event.value);
                return true;

            case 2:
                Apps::SettingsApp::scroll(event.value);
                return true;

            case 3:
                Apps::TerminalApp::scroll(event.value);
                return true;

            default:
                return false;
        }
    }

    if (action.type !=
        InputManager::ActionType::ForwardMouse)
        return false;

    // Click on release.
    // Double-activating buttons is for villains.
    if (event.type != Events::Type::MouseUp)
        return false;

    const Windows::Rect client =
        Decorations::client_bounds(*window);

    if (!Windows::contains(client, event.x, event.y))
        return false;

    switch (instance->appId) {
        case 1:
            return Apps::FileManager::click(
                client,
                event.x,
                event.y
            );

        case 2: {
            const Apps::SettingsApp::Action result =
                Apps::SettingsApp::click(
                    client,
                    event.x,
                    event.y
                );

            switch (result) {
                case Apps::SettingsApp::Action::LockDesktop:
                    return SessionController::lock_desktop();

                case Apps::SettingsApp::Action::LockMain:
                    return SessionController::lock_main();

                case Apps::SettingsApp::Action::Redraw:
                    return true;

                default:
                    return false;
            }
        }

        case 6:
            return Apps::BrowserApp::click(
                client,
                event.x,
                event.y
            );

        case 4:
            return Apps::SystemMonitor::click(
                client,
                event.x,
                event.y
            );

        default:
            return false;
    }
}

uint32_t running_count() {
    return instanceCount;
}

const Instance* at(uint32_t index) {
    return index < instanceCount
        ? &instances[index]
        : nullptr;
}

} // namespace Astral::Desktop::AppManager