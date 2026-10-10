#pragma once

// The hero's weapons and the robot masters who carry them: the buster and
// six special weapons won from the bosses, their energy cost, how many shots
// may fly at once, and the damage table (every boss is weak to one other
// boss's weapon, in a ring). Pure data.

#include <array>
#include <cstdint>
#include <string_view>

namespace blaster::game {

enum class Weapon : std::uint8_t { Buster, Saw, Spark, Frost, Leaf, Magma, Gale, Count };
inline constexpr int kWeaponCount = static_cast<int>(Weapon::Count);
inline constexpr int kSpecialWeapons = kWeaponCount - 1;

// The six robot masters (in stage-select order) and the fortress's final boss.
enum class BossKind : std::uint8_t { Grinder, Storm, Rime, Thorn, Cinder, Gale, Overseer, Count };
inline constexpr int kBossKinds = static_cast<int>(BossKind::Count);
inline constexpr int kMasters = 6;

// Weapon energy is kept in quarter units, so cheap weapons fire several
// shots per bar unit; the bar shows whole units.
inline constexpr int kEnergyPerUnit = 4;

struct WeaponInfo {
    std::string_view name;  // "SAW BLADE"
    std::string_view shortName; // "SAW" for the HUD
    int cost = 0;           // quarter units per shot
    int maxShots = 3;       // shots of this weapon on screen at once
    BossKind from = BossKind::Count; // the boss who carries it
};

[[nodiscard]] const WeaponInfo& weaponInfo(Weapon weapon);
// The special weapon a robot master gives (Weapon::Buster for the final boss).
[[nodiscard]] Weapon weaponOf(BossKind boss);
// The weapon a boss is weak to.
[[nodiscard]] Weapon weaknessOf(BossKind boss);
[[nodiscard]] std::string_view bossName(BossKind boss);

// Bar units a shot takes from a boss: the buster 1 (2 half charged, 3 fully
// charged), the weakness 4, other weapons 1 (the final boss takes 2 from
// every special weapon).
[[nodiscard]] int bossDamage(BossKind boss, Weapon weapon, int charge);

} // namespace blaster::game
