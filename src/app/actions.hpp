#pragma once

// Blaster's controls as GameLib actions (platform::ActionMap): the player
// rebinds them on the CONTROLS screen (ui::BindingList), they are kept in
// the profile's [keys] section, and the keyboard and any gamepad drive the
// same actions. Pause (Escape, P, Start) is locked: ArcadeMenus owns it.

#include "rules.hpp"

#include <gamelib/platform/actions.hpp>
#include <gamelib/platform/input.hpp>

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

} // namespace blaster::app
