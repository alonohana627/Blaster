#include "profile.hpp"

#include <gamelib/app/bindings.hpp>

#include <algorithm>
#include <bit>
#include <string>

namespace blaster::app {

namespace {

// helpers

constexpr std::string_view kAppName = "Blaster";
constexpr std::string_view kDataDirVariable = "BLASTER_DATA_DIR";
constexpr std::string_view kSection = "campaign";

} // namespace

int Progress::beatenCount() const
{
    return std::popcount(static_cast<unsigned>(beaten & 0x3F));
}

void Progress::read(const gamelib::storage::IniFile& ini)
{
    introCleared = ini.getBool(kSection, "intro_cleared").value_or(false);
    beaten = static_cast<std::uint8_t>(std::clamp<long long>(ini.getInt(kSection, "beaten").value_or(0), 0, 63));
    fortress = static_cast<int>(std::clamp<long long>(ini.getInt(kSection, "fortress").value_or(0), 0, game::kFortressStages));
    tanks = static_cast<int>(std::clamp<long long>(ini.getInt(kSection, "tanks").value_or(0), 0, game::kMaxTanks));
    finished = ini.getBool(kSection, "finished").value_or(false);
    playTicks = std::max<long long>(0, ini.getInt(kSection, "play_ticks").value_or(0));
    deaths = static_cast<int>(std::max<long long>(0, ini.getInt(kSection, "deaths").value_or(0)));
    destroyed = static_cast<int>(std::max<long long>(0, ini.getInt(kSection, "destroyed").value_or(0)));
    tanksUsed = static_cast<int>(std::max<long long>(0, ini.getInt(kSection, "tanks_used").value_or(0)));
    // The fortress opens only after all six; an edited file cannot skip ahead.
    if (!fortressOpen()) {
        fortress = 0;
    }
    gamelib::app::readActionBindings(ini, actions);
}

void Progress::write(gamelib::storage::IniFile& ini) const
{
    ini.set(kSection, "intro_cleared", introCleared ? "true" : "false");
    ini.set(kSection, "beaten", std::to_string(beaten));
    ini.set(kSection, "fortress", std::to_string(fortress));
    ini.set(kSection, "tanks", std::to_string(tanks));
    ini.set(kSection, "finished", finished ? "true" : "false");
    ini.set(kSection, "play_ticks", std::to_string(playTicks));
    ini.set(kSection, "deaths", std::to_string(deaths));
    ini.set(kSection, "destroyed", std::to_string(destroyed));
    ini.set(kSection, "tanks_used", std::to_string(tanksUsed));
    gamelib::app::writeActionBindings(actions, ini);
}

ProfileFile openProfile()
{
    return gamelib::app::openArcadeProfile<Progress>(kAppName, kDataDirVariable);
}

} // namespace blaster::app
