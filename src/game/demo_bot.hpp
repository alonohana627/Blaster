#pragma once

// The demo player: a bot that drives the same controller as the player
// through a stage (the attract mode plays the intro). Each room gets a
// distance map to the way into the next room (a flood fill through the
// open cells, so it leads up ladders and down shafts); every few ticks on
// the ground or a ladder the bot tries a handful of plans - walk either way
// or stand, jumping for 0 to 26 ticks, climb, slide - on a copy of the
// session for most of a second (GameLib's look-ahead planner) and takes the
// one that gets closest to that way without being hurt. It fires at
// enemies in line with it. Deterministic: the same session gives the same
// inputs.

#include "rules.hpp"
#include "session.hpp"

#include <gamelib/ai/planner.hpp>
#include <gamelib/core/grid.hpp>

#include <optional>
#include <span>

namespace blaster::game {

class DemoBot {
public:
    struct Plan {
        int move = 1;       // -1, 0, 1 while the plan runs
        int walkTicks = 0;  // without a jump: stop after this many ticks (0: never)
        int jumpDelay = 0;  // ticks before the jump
        int jumpTicks = 0;  // then hold jump this long (0: no jump)
        int climb = 0;      // hold up (-1) or down (1)
        bool slide = false; // start with a slide
    };

    [[nodiscard]] Input next(const Session& session);

private:
    void prepare(const Session& session);
    [[nodiscard]] float distanceAt(const Session& session, gamelib::core::Vec2 point) const;

    gamelib::core::Grid<int> m_distance{1, 1, -1};
    int m_room = -1;
    Plan m_plan;
    int m_left = 0;      // ticks the current plan still runs
    int m_ticks = 0;     // ticks into the current plan
    bool m_jumpHeld = false;
    bool m_shootHeld = false;
    int m_shootCooldown = 0;
};

} // namespace blaster::game
