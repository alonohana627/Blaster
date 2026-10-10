#include "actions.hpp"

#include <optional>

namespace blaster::app {

namespace {

// helpers

using gamelib::platform::ActionId;
using gamelib::platform::GamepadButton;
using gamelib::platform::Key;

} // namespace

gamelib::platform::ActionMap makeActions()
{
    gamelib::platform::ActionMap map;
    map.add({.name = "left", .label = "LEFT", .category = "MOVE", .keys = {Key::Left, Key::A},
             .buttons = {GamepadButton::DpadLeft, GamepadButton::LeftStickLeft}});
    map.add({.name = "right", .label = "RIGHT", .category = "MOVE", .keys = {Key::Right, Key::D},
             .buttons = {GamepadButton::DpadRight, GamepadButton::LeftStickRight}});
    map.add({.name = "up", .label = "UP / CLIMB", .category = "MOVE", .keys = {Key::Up, Key::W},
             .buttons = {GamepadButton::DpadUp, GamepadButton::LeftStickUp}});
    map.add({.name = "down", .label = "DOWN", .category = "MOVE", .keys = {Key::Down, Key::S},
             .buttons = {GamepadButton::DpadDown, GamepadButton::LeftStickDown}});
    map.add({.name = "jump", .label = "JUMP", .category = "ACTION", .keys = {Key::Z, Key::Space, Key::K},
             .buttons = {GamepadButton::A}});
    map.add({.name = "shoot", .label = "SHOOT", .category = "ACTION", .keys = {Key::X, Key::J},
             .buttons = {GamepadButton::X}});
    map.add({.name = "slide", .label = "SLIDE", .category = "ACTION", .keys = {Key::C, Key::L},
             .buttons = {GamepadButton::B}});
    map.add({.name = "weapon_prev", .label = "PREV WEAPON", .category = "WEAPONS", .keys = {Key::Q},
             .buttons = {GamepadButton::LeftBumper}});
    map.add({.name = "weapon_next", .label = "NEXT WEAPON", .category = "WEAPONS", .keys = {Key::E},
             .buttons = {GamepadButton::RightBumper}});
    map.add({.name = "pause", .label = "PAUSE", .category = "SYSTEM", .keys = {Key::Escape, Key::P},
             .buttons = {GamepadButton::Start}, .locked = true});
    return map;
}

game::Input readInput(const gamelib::platform::ActionMap& actions, const gamelib::platform::InputState& input)
{
    game::Input in;
    const auto down = [&](const Action a) { return actions.isDown(input, actionId(a)); };
    const auto pressed = [&](const Action a) { return actions.wasPressed(input, actionId(a)); };
    in.move = (down(Action::Right) ? 1 : 0) - (down(Action::Left) ? 1 : 0);
    in.climb = (down(Action::Down) ? 1 : 0) - (down(Action::Up) ? 1 : 0);
    in.jumpPressed = pressed(Action::Jump);
    in.jumpHeld = in.jumpPressed || down(Action::Jump);
    in.shootPressed = pressed(Action::Shoot);
    in.shootHeld = in.shootPressed || down(Action::Shoot);
    in.slidePressed = pressed(Action::Slide);
    in.weaponPrev = pressed(Action::WeaponPrev);
    in.weaponNext = !in.weaponPrev && pressed(Action::WeaponNext);
    return in;
}

std::string withKeys(const std::string_view text, const gamelib::platform::ActionMap& actions)
{
    std::string out;
    std::size_t i = 0;
    while (i < text.size()) {
        const std::size_t open = text.find('{', i);
        if (open == std::string_view::npos) {
            out += text.substr(i);
            break;
        }
        const std::size_t close = text.find('}', open);
        if (close == std::string_view::npos) {
            out += text.substr(i);
            break;
        }
        out += text.substr(i, open - i);
        const std::string_view name = text.substr(open + 1, close - open - 1);
        std::string label = "?";
        if (const std::optional<ActionId> id = actions.find(name)) {
            // The first default key still bound to it (Z for jump, not K), else any key it has.
            std::optional<Key> shown;
            for (const Key k : actions.info(*id).keys) {
                if (!shown && actions.actionOn(k) == *id) {
                    shown = k;
                }
            }
            const std::vector<Key> keys = actions.keysOf(*id);
            if (!shown && !keys.empty()) {
                shown = keys.front();
            }
            if (shown) {
                label = std::string{gamelib::platform::keyLabel(*shown)};
            }
        }
        out += "[" + label + "]";
        i = close + 1;
    }
    return out;
}

Action actionOf(const gamelib::platform::ActionMap& actions, const Key key)
{
    const ActionId id = actions.actionOn(key);
    return id == gamelib::platform::kNoAction ? Action::Count : static_cast<Action>(id);
}

Action actionOf(const gamelib::platform::ActionMap& actions, const GamepadButton button)
{
    const ActionId id = actions.actionOn(button);
    return id == gamelib::platform::kNoAction ? Action::Count : static_cast<Action>(id);
}

} // namespace blaster::app
