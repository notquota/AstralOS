
#include "shortcuts.h"

namespace Astral::Desktop::Shortcuts {

Action resolve(const KeyEvent& event) {
    if (!event.pressed)
        return Action::None;

    const uint32_t mods = event.modifiers;
    const uint32_t key = event.key;

    const bool super = (mods & Super) != 0;
    const bool ctrl = (mods & Ctrl) != 0;
    const bool alt = (mods & Alt) != 0;

    if (super && (key == 'l' || key == 'L'))
        return Action::LockDesktop;

    if (super && (key == 'e' || key == 'E'))
        return Action::ToggleLauncher;

    if (super && (key == 't' || key == 'T'))
        return Action::OpenTerminal;

    if (super && (key == 'i' || key == 'I'))
        return Action::OpenSettings;

    if (super && (key == 'd' || key == 'D'))
        return Action::ShowDesktop;

    if (alt && key == 9)
        return Action::SwitchWindow;

    if (alt && (key == 0x73 || key == 'q' || key == 'Q'))
        return Action::CloseWindow;

    if (super && (key == 'm' || key == 'M'))
        return Action::MinimizeWindow;

    if (super && (key == 'f' || key == 'F'))
        return Action::MaximizeWindow;

    if (ctrl && alt && (key == 't' || key == 'T'))
        return Action::OpenTerminal;

    return Action::None;
}

}
