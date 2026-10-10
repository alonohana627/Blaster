#pragma once

// Blaster's numbers: the tile grid and the one-screen rooms, the tick rate,
// the hero's boxes and movement tuning (GameLib's platformer controller set
// up like a classic 8-bit action hero: instant run, a fixed-gravity jump cut
// on release, ladders, no coyote tricks beyond a few ticks), health, weapon
// energy, timers and the per-tick input. Every rule runs on whole ticks at
// kTickRate, so the same per-tick input always gives the same run.

#include <gamelib/core/math.hpp>
#include <gamelib/core/platformer.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace blaster::game {

inline constexpr int kTickRate = 60;
inline constexpr float kTile = 16.0f;   // world units per tile
inline constexpr int kRoomRows = 15;    // every room is one screen tall
inline constexpr int kMinRoomColumns = 27;
inline constexpr float kViewWidth = 1920.0f / 4.5f; // the camera's width in world units (26.7 tiles)
inline constexpr float kViewHeight = kRoomRows * kTile; // 240

// The hero's collision boxes: standing and sliding (under one-tile gaps).
inline constexpr gamelib::core::Vec2 kHeroSize{12.0f, 22.0f};
inline constexpr gamelib::core::Vec2 kSlideSize{12.0f, 12.0f};

// Health and energy, in bar units.
inline constexpr int kMaxHealth = 28;
inline constexpr int kMaxEnergy = 28;
inline constexpr int kBossHealth = 28;
inline constexpr int kStartLives = 3;
inline constexpr int kMaxLives = 9;
inline constexpr int kMaxTanks = 4;

// Timers, in ticks.
inline constexpr int kHurtStunTicks = 22;        // knocked back, no control
inline constexpr int kHurtInvincibleTicks = 80;  // flickering after a hit (stun included)
inline constexpr int kSlideTicks = 24;
inline constexpr int kShootPoseTicks = 16;
inline constexpr int kChargeHalfTicks = 32;      // holding fire this long: a bigger shot on release
inline constexpr int kChargeFullTicks = 84;      // and this long: the full charge
inline constexpr int kScrollTicks = 44;          // a room-to-room camera scroll
inline constexpr int kDeathTicks = 150;          // the burst and the pause before the attempt ends
inline constexpr int kGateTicks = 24;            // a boss door opening or closing
inline constexpr int kBossIntroTicks = 70;       // the boss lands and poses
inline constexpr int kBossFillTicks = 2;         // one boss health unit per this many ticks while filling
inline constexpr int kBossDownTicks = 180;       // explosions before the stage is cleared
inline constexpr int kReadyTicks = 70;           // "READY" before the hero beams in

// Movement (units per tick).
inline constexpr float kRunSpeed = 1.375f;
inline constexpr float kSlideSpeed = 2.6f;
inline constexpr float kClimbSpeed = 1.3f;
inline constexpr float kKnockbackSpeed = 0.7f;
inline constexpr float kBusterSpeed = 5.0f;

// One tick of player input, as recorded for replays: after key mapping,
// never raw keys.
struct Input {
    int move = 0;               // -1 left, 0, 1 right
    int climb = 0;              // -1 up, 0, 1 down
    bool jumpHeld = false;      // held this tick
    bool jumpPressed = false;   // pressed this tick (also held)
    bool shootHeld = false;
    bool shootPressed = false;
    bool slidePressed = false;
    bool weaponPrev = false;    // pressed this tick
    bool weaponNext = false;

    friend bool operator==(const Input&, const Input&) = default;
};

// Six characters per tick: move L/R/., climb U/D/., jump h/p/., shoot
// h/p/., slide s/., weapon </>/. (p is "pressed this tick", which implies held).
[[nodiscard]] std::string encodeInput(const Input& input);
[[nodiscard]] std::optional<Input> decodeInput(std::string_view text);

// Bumped whenever a rule or the intro stage changes, so stale recordings are refused.
inline constexpr std::uint32_t kRulesVersion = 1;

// The hero's movement: on the ground and in the air alike (a classic action
// hero steers freely in the air), icy floors slow the start and the stop.
[[nodiscard]] gamelib::core::PlatformerTuning heroTuning(bool icy);
// The slide: the same body pushed at kSlideSpeed in the facing direction.
[[nodiscard]] gamelib::core::PlatformerTuning slideTuning();

} // namespace blaster::game
