#pragma once

// What Blaster keeps between sessions, in blaster.ini in the user data
// folder (BLASTER_DATA_DIR moves it): GameLib's standard settings and
// bests, the key bindings ([keys], GameLib's action-binding text), and the
// campaign - the intro cleared, the robot masters beaten (their weapons
// come with them), the fortress stages cleared, the energy tanks carried
// and the campaign's statistics for the ending.

#include "actions.hpp"
#include "stages.hpp"
#include "weapons.hpp"

#include <gamelib/app/arcade_profile.hpp>
#include <gamelib/platform/actions.hpp>
#include <gamelib/storage/ini.hpp>

#include <cstdint>

namespace blaster::app {

struct Progress {
    bool introCleared = false;
    std::uint8_t beaten = 0; // robot masters beaten (bit = BossKind)
    int fortress = 0;        // fortress stages cleared, 0..3
    int tanks = 0;
    bool finished = false;   // the Overseer was beaten once
    // The campaign's statistics, shown in the ending.
    std::int64_t playTicks = 0;
    int deaths = 0;
    int destroyed = 0;
    int tanksUsed = 0;
    gamelib::platform::ActionMap actions = makeActions();

    // The special weapons owned: bit i is Weapon i + 1, the weapon of BossKind i.
    [[nodiscard]] std::uint8_t weapons() const { return beaten; }
    [[nodiscard]] bool hasBeaten(game::BossKind boss) const { return (beaten & (1u << static_cast<int>(boss))) != 0; }
    [[nodiscard]] int beatenCount() const;
    [[nodiscard]] bool fortressOpen() const { return beatenCount() >= game::kMasters; }

    void read(const gamelib::storage::IniFile& ini);
    void write(gamelib::storage::IniFile& ini) const;
    friend bool operator==(const Progress&, const Progress&) = default;
};

using ProfileFile = gamelib::app::ArcadeProfileFileOf<Progress>;

[[nodiscard]] ProfileFile openProfile();

// The best campaign time (ticks) in the bests.
inline constexpr std::string_view kBestTime = "campaign_ticks";

} // namespace blaster::app
