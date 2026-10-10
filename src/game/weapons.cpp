#include "weapons.hpp"

namespace blaster::game {

namespace {

// helpers

constexpr std::array<WeaponInfo, kWeaponCount> kWeapons{{
    {.name = "BUSTER", .shortName = "P", .cost = 0, .maxShots = 3, .from = BossKind::Count},
    {.name = "SAW BLADE", .shortName = "SAW", .cost = 2, .maxShots = 2, .from = BossKind::Grinder},
    {.name = "SPARK SHOT", .shortName = "SPK", .cost = 2, .maxShots = 3, .from = BossKind::Storm},
    {.name = "ICE LANCE", .shortName = "ICE", .cost = 3, .maxShots = 2, .from = BossKind::Rime},
    {.name = "LEAF SPREAD", .shortName = "LEF", .cost = 4, .maxShots = 6, .from = BossKind::Thorn},
    {.name = "MAGMA LOB", .shortName = "MAG", .cost = 4, .maxShots = 2, .from = BossKind::Cinder},
    {.name = "GALE WING", .shortName = "GAL", .cost = 4, .maxShots = 2, .from = BossKind::Gale},
}};

} // namespace

const WeaponInfo& weaponInfo(const Weapon weapon)
{
    return kWeapons[static_cast<std::size_t>(weapon)];
}

Weapon weaponOf(const BossKind boss)
{
    if (boss == BossKind::Overseer || boss == BossKind::Count) {
        return Weapon::Buster;
    }
    return static_cast<Weapon>(static_cast<int>(boss) + 1);
}

Weapon weaknessOf(const BossKind boss)
{
    // The ring: Thorn burns (magma), Cinder freezes (ice), Rime is cut (saw),
    // Grinder shorts (spark), Storm is blown apart (gale), Gale is tangled (leaf).
    switch (boss) {
    case BossKind::Grinder: return Weapon::Spark;
    case BossKind::Storm: return Weapon::Gale;
    case BossKind::Rime: return Weapon::Saw;
    case BossKind::Thorn: return Weapon::Magma;
    case BossKind::Cinder: return Weapon::Frost;
    case BossKind::Gale: return Weapon::Leaf;
    case BossKind::Overseer:
    case BossKind::Count: break;
    }
    return Weapon::Count;
}

std::string_view bossName(const BossKind boss)
{
    switch (boss) {
    case BossKind::Grinder: return "GEARGRINDER";
    case BossKind::Storm: return "STORMCALLER";
    case BossKind::Rime: return "RIMEFANG";
    case BossKind::Thorn: return "THORNWOOD";
    case BossKind::Cinder: return "CINDERMAW";
    case BossKind::Gale: return "GALEWING";
    case BossKind::Overseer: return "THE OVERSEER";
    case BossKind::Count: break;
    }
    return "";
}

int bossDamage(const BossKind boss, const Weapon weapon, const int charge)
{
    if (weapon == Weapon::Buster) {
        return 1 + charge;
    }
    if (boss == BossKind::Overseer) {
        return 2;
    }
    return weapon == weaknessOf(boss) ? 4 : 1;
}

} // namespace blaster::game
