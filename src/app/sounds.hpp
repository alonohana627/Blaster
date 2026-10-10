#pragma once

// Blaster's sound effects and music, all synthesized with GameLib's 8-bit
// synth at startup: no audio files. Every stage has its theme, the robot
// masters share a boss theme, the Overseer has his own.

#include "stages.hpp"

#include <gamelib/audio/music.hpp>
#include <gamelib/audio/player.hpp>
#include <gamelib/audio/sound_bank.hpp>
#include <gamelib/audio/synth.hpp>

#include <cstddef>
#include <cstdint>

namespace blaster::app {

enum class Sfx : std::uint8_t {
    Shoot,
    ShootHalf,
    ShootFull,
    Charge,      // the rising loop while charging
    ChargeReady,
    Weapon,      // a special weapon fired
    Land,
    Slide,
    EnemyHit,
    Deflect,
    Explosion,   // an enemy destroyed
    Hurt,
    Death,
    Pickup,
    HealthTick,  // a bar refilling
    ExtraLife,
    Tank,
    Checkpoint,
    DoorOpen,
    BossFill,
    BossHit,
    BossDown,
    BossLand,
    Strike,      // lightning, thorns, a laser
    EnemyShot,
    Bomb,
    WeaponSwitch,
    WeaponGet,
    Ladder,
    Beam,        // beaming in or out
    Ready,
    MenuMove,
    MenuSelect,
    MenuBack,
    GameOver,
    Unlock,
    Count
};
inline constexpr std::size_t kSfxCount = static_cast<std::size_t>(Sfx::Count);

[[nodiscard]] gamelib::audio::SoundRecipe sfxRecipe(Sfx sfx);

enum class Tune : std::uint8_t {
    Title,
    Select,
    Lab,
    Factory,
    Storm,
    Ice,
    Forest,
    Magma,
    Sky,
    Fortress,
    Boss,
    FinalBoss,
    WeaponGet,
    Ending,
    Count
};
inline constexpr std::size_t kTuneCount = static_cast<std::size_t>(Tune::Count);

[[nodiscard]] gamelib::audio::Song tune(Tune tune);
[[nodiscard]] Tune stageTune(game::Theme theme);

using Music = gamelib::audio::MusicBank<Tune, kTuneCount>;
using Sounds = gamelib::audio::SoundBank<Sfx, kSfxCount>;

} // namespace blaster::app
