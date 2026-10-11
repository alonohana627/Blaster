#include "actions.hpp"


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

} // namespace blaster::app
