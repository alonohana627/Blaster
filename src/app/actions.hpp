#pragma once

// Blaster's controls as GameLib actions (platform::ActionMap): the player
// rebinds them on the CONTROLS screen (ui::BindingList), they are kept in
// the profile's [keys] section, and the keyboard and any gamepad drive the
// same actions. Pause (Escape, P, Start) is locked: ArcadeMenus owns it.

#include "rules.hpp"

#include <gamelib/platform/actions.hpp>
#include <gamelib/platform/input.hpp>

#include <string>
#include <string_view>

namespace blaster::app {

enum class Action : std::uint8_t { Left, Right, Up, Down, Jump, Shoot, Slide, WeaponPrev, WeaponNext, Pause, Count };

// Every action registered with its default keys and buttons, in Action order.
[[nodiscard]] gamelib::platform::ActionMap makeActions();

[[nodiscard]] constexpr gamelib::platform::ActionId actionId(const Action action)
{
    return static_cast<gamelib::platform::ActionId>(action);
}

// The rules' input for this tick from the held and pressed actions.
[[nodiscard]] game::Input readInput(const gamelib::platform::ActionMap& actions, const gamelib::platform::InputState& input);

// "{jump}" and the other action names in braces replaced by the action's
// first key ("Z"), for the teaching hints.
[[nodiscard]] std::string withKeys(std::string_view text, const gamelib::platform::ActionMap& actions);

// The action a key or button is bound to (Action::Count when none).
[[nodiscard]] Action actionOf(const gamelib::platform::ActionMap& actions, gamelib::platform::Key key);
[[nodiscard]] Action actionOf(const gamelib::platform::ActionMap& actions, gamelib::platform::GamepadButton button);

} // namespace blaster::app
