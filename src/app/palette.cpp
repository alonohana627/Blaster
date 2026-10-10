#include "palette.hpp"

#include <array>

namespace blaster::app {

const ThemePalette& themePalette(const game::Theme theme)
{
    // SkyTop, SkyLow, Far, FarLight, Near, NearLight, Ground, GroundEdge,
    // Block, BlockEdge, Platform, Ladder, Accent
    static const std::array<ThemePalette, game::kThemeCount> palettes{
        // Lab: a teal dusk over a sleeping city.
        ThemePalette::fromHex({0x0E1A3A, 0x3A4E8A, 0x18274E, 0xFFD27A, 0x1F3A5A, 0x62C6D8, 0x3C4E6E, 0x8FD4E8,
                               0x5A6A8E, 0xB8C8E8, 0x8A9AC0, 0xD8E0F0, 0x5FE0FF}),
        // Factory: rust and steel under a smoky orange sky.
        ThemePalette::fromHex({0x3A1A14, 0xC0582A, 0x2A1C1C, 0xFF9A3A, 0x4A3028, 0xE07A3A, 0x6A5048, 0xD8A070,
                               0x7A7E8A, 0xC8CCD8, 0xA08060, 0xD8C090, 0xFF8A2A}),
        // Storm: violet clouds, rain and lightning.
        ThemePalette::fromHex({0x0C0A24, 0x3A2E6A, 0x1A1640, 0xFFE860, 0x262058, 0x8A7AE8, 0x3A3A6A, 0x9A9AE0,
                               0x4A4A80, 0xC0C0FF, 0x7A70C0, 0xE0E0FF, 0xFFE860}),
        // Ice: pale caves of blue and white.
        ThemePalette::fromHex({0x2A5A9A, 0xBFE8FF, 0x6A9AD0, 0xFFFFFF, 0x8ABCE8, 0xF0FAFF, 0x5A8AC8, 0xE8F8FF,
                               0x9AD0F0, 0xFFFFFF, 0xA8D8F8, 0xE8F4FF, 0x9FF0FF}),
        // Forest: deep greens under a golden canopy.
        ThemePalette::fromHex({0x1A3A2A, 0x7AB86A, 0x1E4A2E, 0xC8E87A, 0x2E5A30, 0x8ED06A, 0x5A3E28, 0x7AC85A,
                               0x7A5A38, 0xC8A070, 0x8A6A40, 0xD8B880, 0xB8FF6A}),
        // Magma: black rock over rivers of fire.
        ThemePalette::fromHex({0x1A0608, 0x8A1E10, 0x2A0E0C, 0xFF6A1A, 0x3A1410, 0xFF9A2A, 0x3A2420, 0xFF7A3A,
                               0x4A3A3A, 0xA88070, 0x6A4A40, 0xC8A080, 0xFF6A1A}),
        // Sky: bright blue, white clouds, gold trim.
        ThemePalette::fromHex({0x3A7AE8, 0xC8E8FF, 0xF4F8FF, 0xFFFFFF, 0xE8F0FF, 0xFFF4C8, 0xE8ECF8, 0xFFD23F,
                               0xB8C8E8, 0xFFFFFF, 0xF4D890, 0xFFF0C0, 0xFFD23F}),
        // Fortress: dark violet steel with red warning lights.
        ThemePalette::fromHex({0x0A0614, 0x2A1A3A, 0x160E24, 0xFF3A4A, 0x221636, 0x8A4AE0, 0x3A3048, 0xB898E8,
                               0x4A4060, 0xD0C0F0, 0x6A5A80, 0xC8B8E0, 0xFF3A4A}),
    };
    return palettes[static_cast<std::size_t>(theme)];
}

HeroColors heroColors(const game::Weapon weapon)
{
    switch (weapon) {
    case game::Weapon::Buster: return {srgb(0x2F7BFF), srgb(0x6FE0FF)};
    case game::Weapon::Saw: return {srgb(0x7E8494), srgb(0xFF9A3A)};
    case game::Weapon::Spark: return {srgb(0x6A4AE0), srgb(0xFFE14D)};
    case game::Weapon::Frost: return {srgb(0x3A8CFF), srgb(0xD8FAFF)};
    case game::Weapon::Leaf: return {srgb(0x2FA84A), srgb(0xD8C070)};
    case game::Weapon::Magma: return {srgb(0xD8342A), srgb(0xFFC23A)};
    case game::Weapon::Gale: return {srgb(0x4AA8F0), srgb(0xF8FCFF)};
    case game::Weapon::Count: break;
    }
    return {srgb(0x2F7BFF), srgb(0x6FE0FF)};
}

HeroColors bossColors(const game::BossKind boss)
{
    switch (boss) {
    case game::BossKind::Grinder: return {srgb(0x8A8E9C), srgb(0xFF8A2A)};
    case game::BossKind::Storm: return {srgb(0x5A3AC8), srgb(0xFFE860)};
    case game::BossKind::Rime: return {srgb(0x9AD8FF), srgb(0xFFFFFF)};
    case game::BossKind::Thorn: return {srgb(0x3E8A3A), srgb(0xC8E87A)};
    case game::BossKind::Cinder: return {srgb(0x3A2A2A), srgb(0xFF6A1A)};
    case game::BossKind::Gale: return {srgb(0xE8ECF8), srgb(0x4AA8F0)};
    case game::BossKind::Overseer: return {srgb(0x3A2A58), srgb(0xFF3A4A)};
    case game::BossKind::Count: break;
    }
    return {srgb(0x888888), srgb(0xFFFFFF)};
}

} // namespace blaster::app
