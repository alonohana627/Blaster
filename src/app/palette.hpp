#pragma once

// Blaster's colors: a crisp palette per stage theme (the lab at dusk, the
// rusty factory, the storm, the ice caves, the forest, the magma works, the
// sky fortress, the Overseer's fortress) as GameLib core::Palette by role,
// and the hero's two colors per equipped weapon (he recolors on a switch).
// Enemies, shots and the HUD keep fixed colors so they always read the same.

#include "stages.hpp"
#include "weapons.hpp"

#include <gamelib/core/color.hpp>

#include <cstddef>
#include <cstdint>

namespace blaster::app {

using gamelib::core::Color;
using gamelib::core::srgb;

enum class Role : std::uint8_t {
    SkyTop,
    SkyLow,
    Far,        // the far layer's silhouettes
    FarLight,   // its highlights (windows, snow, sparks)
    Near,       // the near parallax layer
    NearLight,
    Ground,     // '#' fill
    GroundEdge, // '#' rims and caps
    Block,      // 'B'
    BlockEdge,
    Platform,   // '=' and lifts
    Ladder,
    Accent,     // stage name, beacons
    Count
};
inline constexpr std::size_t kRoleCount = static_cast<std::size_t>(Role::Count);
using ThemePalette = gamelib::core::Palette<Role, kRoleCount>;

[[nodiscard]] const ThemePalette& themePalette(game::Theme theme);
[[nodiscard]] constexpr float themeIndex(const game::Theme theme) { return static_cast<float>(theme); }

// The hero's main and secondary color with a weapon equipped.
struct HeroColors {
    Color main;
    Color trim;
};
[[nodiscard]] HeroColors heroColors(game::Weapon weapon);
// A boss's own two colors (its body and its glow), also for its portrait.
[[nodiscard]] HeroColors bossColors(game::BossKind boss);

// Fixed colors.
inline const Color kText = srgb(0xF4F6FF);
inline const Color kTextDim = srgb(0xA8B0D0);
inline const Color kInk = srgb(0x07060F);
inline const Color kPanel = srgb(0x12132A);
inline const Color kGold = srgb(0xFFD23F);
inline const Color kHealth = srgb(0xFFE7A0);
inline const Color kDanger = srgb(0xFF4A5A);
inline const Color kEnemyShot = srgb(0xFFB040);

} // namespace blaster::app
