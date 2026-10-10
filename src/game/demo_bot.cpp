#include "demo_bot.hpp"

#include <gamelib/core/grid_search.hpp>

#include <array>
#include <cmath>
#include <vector>

namespace blaster::game {

namespace {

// helpers

using gamelib::core::Cell;
using gamelib::core::Vec2;
using Plan = DemoBot::Plan;

constexpr int kHorizon = 72;  // ticks each plan is tried for
constexpr int kCommit = 8;    // ticks a chosen plan runs before the next look ahead
constexpr float kFar = 1e4f;  // the distance of a cell the map does not reach

// Walk either way (jumping at once or a little later), stand, jump on the
// spot, slide, climb.
constexpr std::array<Plan, 22> kGroundPlans = [] {
    std::array<Plan, 22> plans{};
    std::size_t i = 0;
    for (const int move : {1, -1}) {
        plans[i++] = {.move = move, .walkTicks = 12};
        plans[i++] = {.move = move, .walkTicks = 36};
        for (const int delay : {0, 12}) {
            for (const int hold : {8, 16, 26}) {
                plans[i++] = {.move = move, .jumpDelay = delay, .jumpTicks = hold};
            }
        }
    }
    plans[i++] = {.move = 0};
    plans[i++] = {.move = 0, .jumpTicks = 26};
    plans[i++] = {.move = 1, .slide = true};
    plans[i++] = {.move = -1, .slide = true};
    plans[i++] = {.move = 0, .climb = -1};
    plans[i++] = {.move = 0, .climb = 1};
    return plans;
}();
constexpr std::array<Plan, 4> kLadderPlans{{
    {.move = 0, .climb = -1}, {.move = 0, .climb = 1}, {.move = 1, .jumpTicks = 1}, {.move = -1, .jumpTicks = 1},
}};
} // namespace

void DemoBot::prepare(const Session& session)
{
    m_room = session.roomIndex();
    const std::span<const Room> rooms = session.rooms();
    const Room& here = rooms[static_cast<std::size_t>(m_room)];
    const gamelib::core::TileLayer& layer = session.map().layer(0);
    const gamelib::core::TileWorld world = session.tileWorld();
    const bool last = m_room + 1 >= static_cast<int>(rooms.size());
    const gamelib::core::CellRange next = last ? gamelib::core::CellRange{} : rooms[static_cast<std::size_t>(m_room + 1)].cells;
    const auto open = [&](const Cell c) {
        const gamelib::core::TileId id = layer.at(c);
        return id == 'D' || (world.shapeAt(c) != gamelib::core::TileShape::Solid && id != '^' && id != '~');
    };
    // The way into the next room: its open cells beside this room.
    std::vector<Cell> starts;
    if (!last) {
        for (int y = next.min.y; y <= next.max.y; ++y) {
            for (int x = next.min.x; x <= next.max.x; ++x) {
                const Cell c{x, y};
                bool beside = false;
                for (const Cell d : gamelib::core::neighborOffsets(gamelib::core::Neighbors::Four)) {
                    beside = beside || here.cells.contains(c + d);
                }
                if (beside && open(c)) {
                    starts.push_back(c);
                }
            }
        }
    }
    if (starts.empty()) {
        // The last room: its middle, on the floor.
        starts.push_back({(here.cells.min.x + here.cells.max.x) / 2, here.cells.max.y - 4});
    }
    m_distance = gamelib::core::floodFill(layer, std::span<const Cell>{starts}, gamelib::core::Neighbors::Four,
                                          [&](const Cell c) {
                                              const bool inside = here.cells.contains(c) || (!last && next.contains(c));
                                              return inside && open(c) ? gamelib::core::Reach::Spread
                                                                       : gamelib::core::Reach::Blocked;
                                          });
}

float DemoBot::distanceAt(const Session& session, const Vec2 point) const
{
    (void)session;
    const Cell c = gamelib::core::cellAt(point, kTile);
    if (!m_distance.inside(c)) {
        return kFar;
    }
    const int d = m_distance.at(c);
    return d < 0 ? kFar : static_cast<float>(d);
}

Input DemoBot::next(const Session& session)
{
    Input input;
    if (session.phase() != Phase::Playing) {
        m_left = 0;
        m_jumpHeld = false;
        m_shootHeld = false;
        return input;
    }
    if (session.roomIndex() != m_room) {
        prepare(session);
        m_left = 0;
    }
    const Hero& hero = session.hero();
    const gamelib::core::PlatformerBody& body = hero.body;

    // Look ahead on the ground or a ladder once the last plan has run.
    if (m_left <= 0 && (body.grounded || body.climbing)) {
        struct Trial {
            Session session;
            int room = 0;
            int health = 0;
            bool airborne = false;
            bool settled = false;
        };
        const DemoBot& self = *this;
        const gamelib::ai::PlanRules<Trial, Plan> rules{
            .step = [](Trial& trial, const Plan& plan, const int t) -> std::optional<float> {
                Input in;
                // Walk (and jump) as planned, then stand: a jump ends on landing, a walk after its ticks.
                const auto& body = trial.session.hero().body;
                if (!body.grounded && !body.climbing) {
                    trial.airborne = true;
                } else if ((plan.jumpTicks > 0 && trial.airborne) || (plan.jumpTicks == 0 && plan.walkTicks > 0 && t >= plan.walkTicks)) {
                    trial.settled = true;
                }
                in.move = trial.settled ? 0 : plan.move;
                in.climb = plan.climb;
                in.jumpHeld = t >= plan.jumpDelay && t < plan.jumpDelay + plan.jumpTicks;
                in.jumpPressed = t == plan.jumpDelay && plan.jumpTicks > 0;
                in.slidePressed = t == 0 && plan.slide;
                trial.session.tick(in);
                (void)trial.session.events().take();
                const auto tick = static_cast<float>(t);
                switch (trial.session.phase()) {
                case Phase::Dying: return -1e6f + tick;
                case Phase::Over: return trial.session.outcome() == Outcome::Died ? -1e6f + tick : 1e6f;
                case Phase::Scroll: return trial.session.scrollTarget() > trial.room ? 1e5f - tick : -5e5f + tick;
                case Phase::Gate:
                case Phase::BossIntro:
                case Phase::BossDown: return 1e5f - tick;
                default: break;
                }
                if (trial.session.hero().health.current < trial.health) {
                    return -2e5f + tick;
                }
                return std::nullopt;
            },
            .rate = [&self](const Trial& trial, const Plan&) {
                const Hero& h = trial.session.hero();
                if (!h.body.grounded && !h.body.climbing && h.body.velocity.y > 0.5f) {
                    return -2e5f - h.body.box.y; // still falling after all this time: into a pit
                }
                const float d = self.distanceAt(trial.session, h.body.box.center());
                return -d * kTile + (h.body.grounded || h.body.climbing ? 4.0f : 0.0f);
            }};
        const Trial start{.session = session, .room = session.roomIndex(), .health = hero.health.current};
        std::optional<gamelib::ai::PlanChoice> choice;
        if (body.climbing) {
            choice = gamelib::ai::bestPlan(start, std::span<const Plan>{kLadderPlans}, rules, kHorizon, 1.0f);
            m_plan = kLadderPlans[choice->index];
        } else {
            choice = gamelib::ai::bestPlan(start, std::span<const Plan>{kGroundPlans}, rules, kHorizon, 1.0f);
            m_plan = kGroundPlans[choice->index];
        }
        m_left = kCommit;
        m_ticks = 0;
    }

    input.move = m_plan.move;
    input.climb = m_plan.climb;
    if (m_ticks >= m_plan.jumpDelay && m_ticks < m_plan.jumpDelay + m_plan.jumpTicks) {
        input.jumpHeld = true;
        input.jumpPressed = m_ticks == m_plan.jumpDelay && !m_jumpHeld;
    }
    input.slidePressed = m_ticks == 0 && m_plan.slide;
    m_jumpHeld = input.jumpHeld;
    ++m_ticks;
    --m_left;
    if (!body.grounded && !body.climbing && m_left <= 0) {
        m_left = 1; // finish the jump with the same direction, then look again on landing
    }

    // Fire at anything in line ahead.
    const Vec2 me = body.box.center();
    bool target = false;
    const auto inLine = [&](const gamelib::core::Rect& box) {
        const Vec2 c = box.center();
        const float ahead = (c.x - me.x) * static_cast<float>(body.facing);
        return std::abs(c.y - me.y) < 14.0f + box.height / 2.0f && ahead > 0.0f && ahead < 230.0f;
    };
    for (const auto [handle, e] : session.enemies()) {
        target = target || inLine(e.body.box);
    }
    if (const Boss* boss = session.activeBoss(); boss != nullptr) {
        target = target || inLine(boss->body.box);
    }
    if (m_shootCooldown > 0) {
        --m_shootCooldown;
    }
    if (target && m_shootCooldown == 0 && !hero.body.sliding) {
        input.shootPressed = true;
        input.shootHeld = true;
        m_shootCooldown = 9;
    }
    m_shootHeld = input.shootHeld;
    return input;
}

} // namespace blaster::game
