#include "session.hpp"

#include <gamelib/core/collision.hpp>
#include <gamelib/core/replay.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace blaster::game {

namespace {

// helpers

using gamelib::core::Cell;
using gamelib::core::Rect;
using gamelib::core::TileShape;
using gamelib::core::Vec2;

constexpr float kLiftSpeed = 0.6f;
constexpr int kDropLifeTicks = 420;
constexpr int kBeamOutTicks = 120;

const gamelib::core::TileShapes& _shapes()
{
    static const gamelib::core::TileShapes shapes{
        {'#', TileShape::Solid}, {'B', TileShape::Solid},  {'I', TileShape::Solid},
        {'<', TileShape::Solid}, {'>', TileShape::Solid},  {'D', TileShape::Solid},
        {'L', TileShape::Solid}, {'=', TileShape::OneWay}, {'H', TileShape::Ladder},
        {'^', TileShape::Hazard}, {'~', TileShape::Hazard},
    };
    return shapes;
}

EnemyKind _enemyKind(const char symbol)
{
    switch (symbol) {
    case 'c': return EnemyKind::Crawler;
    case 'h': return EnemyKind::Hopper;
    case 'd': return EnemyKind::Diver;
    case 'w': return EnemyKind::ShieldWalker;
    case 't': return EnemyKind::Turret;
    case 'n': return EnemyKind::Nest;
    case 'g': return EnemyKind::Drone;
    case 'r': return EnemyKind::Roller;
    case 'l': return EnemyKind::Lobber;
    case 'p': return EnemyKind::Popper;
    case 'f': return EnemyKind::Wisp;
    case 'b': return EnemyKind::Dropper;
    case 'm': return EnemyKind::MiniBoss;
    default: return EnemyKind::Count;
    }
}

Rect _cellsRect(const gamelib::core::CellRange& range)
{
    return {static_cast<float>(range.min.x) * kTile, static_cast<float>(range.min.y) * kTile,
            static_cast<float>(range.max.x - range.min.x + 1) * kTile,
            static_cast<float>(range.max.y - range.min.y + 1) * kTile};
}

} // namespace

Carry Carry::fresh(const int lives, const int tanks, const std::uint8_t weapons)
{
    Carry carry{.lives = lives, .tanks = tanks, .weapons = weapons};
    carry.energy.fill({kMaxEnergy * kEnergyPerUnit, kMaxEnergy * kEnergyPerUnit});
    return carry;
}

Session::Session(const StageId stage, const Carry& carry)
    : m_stage{stage}
    , m_start{carry}
    , m_lives{carry.lives}
    , m_tanks{carry.tanks}
    , m_rematches{carry.rematches}
    , m_rng{0xB1A5'7000ULL + static_cast<std::uint64_t>(stage) * 977ULL + static_cast<std::uint64_t>(carry.checkpoint + 1)}
{
    if (static_cast<int>(stage) < 0 || static_cast<int>(stage) >= kStageCount) {
        throw std::out_of_range{"Blaster: no such stage"};
    }
    m_hero.weapons = carry.weapons;
    m_hero.energy = carry.energy;
    m_hero.weapon = owns(carry.weapon) ? carry.weapon : Weapon::Buster;
    load();
}

bool Session::owns(const Weapon weapon) const
{
    return weapon == Weapon::Buster || (m_hero.weapons & (1u << (static_cast<int>(weapon) - 1))) != 0;
}

gamelib::core::TileWorld Session::tileWorld() const
{
    return gamelib::core::TileWorld{m_map, 0, _shapes()};
}

void Session::load()
{
    const StageDef& d = def();
    const StageLayout layout = layoutStage(d);
    const std::vector<std::string_view> rows{layout.rows.begin(), layout.rows.end()};
    const std::array<gamelib::core::LayerMapping, 1> layers{gamelib::core::charTile};
    m_map = gamelib::core::TileMap::fromText(rows, kTile, layers, kMarkerSymbols);
    m_rooms.clear();
    for (const gamelib::core::CellRange& range : layout.rooms) {
        m_rooms.push_back(Room{.bounds = _cellsRect(range), .cells = range});
    }
    const auto roomOf = [&](const Cell cell) {
        for (std::size_t i = 0; i < m_rooms.size(); ++i) {
            if (m_rooms[i].cells.contains(cell)) {
                return static_cast<int>(i);
            }
        }
        return 0;
    };
    Vec2 start;
    int startRoom = -1;
    struct Pending {
        int room;
        Vec2 feet;
    };
    std::vector<Pending> checkpoints;
    for (const gamelib::core::TileMarker& marker : m_map.markers()) {
        const int room = roomOf(marker.cell);
        Room& r = m_rooms[static_cast<std::size_t>(room)];
        const Vec2 feet = m_map.cellBottom(marker.cell);
        const Vec2 center = gamelib::core::cellRect(marker.cell, kTile).center();
        switch (marker.symbol) {
        case 'S':
            start = feet;
            startRoom = room;
            break;
        case 'K': checkpoints.push_back({room, feet}); break;
        case 'W': r.windy = true; break;
        case 'M': {
            const Rect box{static_cast<float>(marker.cell.x) * kTile, static_cast<float>(marker.cell.y) * kTile,
                           2.0f * kTile, kTile * 0.5f};
            m_platforms.push_back({.rail = gamelib::core::railFrom(m_map.layer(0), marker.cell, '-', '|', 2, kTile),
                                   .box = box, .start = box, .previous = box,
                                   .id = static_cast<int>(m_platforms.size()), .room = room});
            break;
        }
        case 'F': {
            const Rect box{static_cast<float>(marker.cell.x) * kTile, static_cast<float>(marker.cell.y) * kTile,
                           2.0f * kTile, kTile * 0.5f};
            m_platforms.push_back({.drop = {.box = box}, .falling = true, .box = box, .start = box, .previous = box,
                                   .id = static_cast<int>(m_platforms.size()), .room = room});
            break;
        }
        case 'E':
        case 'U':
        case '$':
        case '&': {
            const PickupKind kind = marker.symbol == 'E'   ? PickupKind::Tank
                                    : marker.symbol == 'U' ? PickupKind::ExtraLife
                                    : marker.symbol == '$' ? PickupKind::BigHealth
                                                           : PickupKind::BigEnergy;
            const int index = static_cast<int>(m_placedTaken.size());
            m_placedTaken.push_back(false);
            const Rect box = Rect::fromBottomCenter(feet, {14.0f, 14.0f});
            m_pickups.push_back({.kind = kind, .body = {.box = box}, .previous = box, .room = room, .placed = index});
            break;
        }
        default:
            if (marker.symbol >= '1' && marker.symbol <= '7') {
                Boss boss;
                boss.kind = static_cast<BossKind>(marker.symbol - '1');
                boss.home = feet;
                boss.room = room;
                boss.vulnerable = gamelib::core::Vulnerable{{}, 1, 30, 0};
                if ((m_rematches & (1u << static_cast<int>(boss.kind))) != 0) {
                    boss.sm.enter(BossState::Dead);
                    boss.hp = 0;
                }
                r.boss = static_cast<int>(m_bosses.size());
                m_bosses.push_back(boss);
            } else if (const EnemyKind kind = _enemyKind(marker.symbol); kind != EnemyKind::Count) {
                r.spawner.add({.kind = static_cast<int>(kind),
                               .position = kind == EnemyKind::MiniBoss ? feet : center,
                               .respawn = kind == EnemyKind::MiniBoss ? gamelib::core::Respawn::OnRoomEntry
                                                                      : gamelib::core::Respawn::WhenScrolledIn});
            }
            break;
        }
    }
    if (startRoom < 0) {
        throw std::logic_error{"Blaster: a stage without a start"};
    }
    // Checkpoints in the order the stage is played: by room, then left to right.
    std::ranges::sort(checkpoints, [](const Pending& a, const Pending& b) {
        return a.room != b.room ? a.room < b.room : a.feet.x < b.feet.x;
    });
    for (const Pending& p : checkpoints) {
        m_checkpoints.push_back({.room = p.room, .feet = p.feet});
    }
    // Boss doors: the 'D' cells of each column of each room.
    const gamelib::core::TileLayer& layer = m_map.layer(0);
    for (const Cell cell : gamelib::core::cellsWith(layer, 'D', gamelib::core::CellOrder::Columns)) {
        const int room = roomOf(cell);
        if (!m_doors.empty() && m_doors.back().room == room && m_doors.back().cells.back().x == cell.x
            && m_doors.back().cells.back().y + 1 == cell.y) {
            m_doors.back().cells.push_back(cell);
        } else {
            m_doors.push_back({.cells = {cell}, .room = room});
        }
    }
    m_doorOpen.assign(m_doors.size(), 0);
    for (const Room& r : m_rooms) {
        m_roomBounds.push_back(r.bounds);
    }

    Vec2 feet = start;
    int room = startRoom;
    if (m_start.checkpoint >= 0 && m_start.checkpoint < static_cast<int>(m_checkpoints.size())) {
        m_checkpoint = m_start.checkpoint;
        feet = m_checkpoints[static_cast<std::size_t>(m_checkpoint)].feet;
        room = m_checkpoints[static_cast<std::size_t>(m_checkpoint)].room;
    }
    placeHero(feet, room);
    enterRoom(room);
}

void Session::placeHero(const Vec2 feet, const int room)
{
    Hero& h = m_hero;
    h.body = {};
    h.body.box = Rect::fromBottomCenter(feet, kHeroSize);
    h.body.grounded = true;
    h.previous = h.body.box;
    h.vulnerable = gamelib::core::Vulnerable{h.body.box, 0, kHurtInvincibleTicks, kHurtStunTicks};
    h.charge.reset();
    h.shootTicks = 0;
    m_room = room;
}

void Session::debugStartAt(const int room, const int column)
{
    if (room < 0 || room >= static_cast<int>(m_rooms.size())) {
        return;
    }
    const Room& r = m_rooms[static_cast<std::size_t>(room)];
    const int x = std::clamp(r.cells.min.x + column, r.cells.min.x, r.cells.max.x);
    const gamelib::core::TileWorld world = tileWorld();
    for (int y = r.cells.min.y + 1; y <= r.cells.max.y; ++y) {
        const TileShape below = world.shapeAt({x, y});
        if ((below == TileShape::Solid || below == TileShape::OneWay) && world.shapeAt({x, y - 1}) == TileShape::Empty) {
            placeHero(m_map.cellBottom({x, y - 1}), room);
            enterRoom(room);
            return;
        }
    }
}

int Session::roomAt(const Vec2 point) const
{
    for (std::size_t i = 0; i < m_rooms.size(); ++i) {
        if (m_rooms[i].bounds.contains(point)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

Rect Session::viewOf(const int room, const Rect heroBox) const
{
    const Rect& b = m_rooms[static_cast<std::size_t>(room)].bounds;
    const float x = std::clamp(heroBox.center().x - kViewWidth / 2.0f, b.left(), b.right() - kViewWidth);
    return {x, b.top(), kViewWidth, kViewHeight};
}

Rect Session::view() const
{
    return viewOf(m_room, m_hero.body.box);
}

const Boss* Session::activeBoss() const
{
    const int index = room().boss;
    if (index < 0) {
        return nullptr;
    }
    const Boss& boss = m_bosses[static_cast<std::size_t>(index)];
    if (boss.sm.is(BossState::Waiting) || (boss.sm.is(BossState::Dead) && boss.sm.ticksIn() > kBossDownTicks)) {
        return nullptr;
    }
    if (boss.sm.is(BossState::Dead) && boss.hp <= 0 && boss.sm.entries() == 1) {
        return nullptr; // beaten in an earlier attempt
    }
    return &boss;
}

void Session::enterRoom(const int room)
{
    m_room = room;
    m_enemies = {};
    m_shots.clear();
    m_warnings.clear();
    m_wind = 0.0f;
    std::erase_if(m_pickups, [](const Pickup& p) { return p.placed < 0; });
    Room& r = m_rooms[static_cast<std::size_t>(room)];
    const Rect v = view();
    for (const int point : r.spawner.reset(v.expanded(8.0f))) {
        spawnEnemy(point);
    }
    // Lifts restart with the room; fallen platforms come back.
    for (Platform& p : m_platforms) {
        if (p.room == room) {
            p.box = p.start;
            p.previous = p.box;
            p.drop = {.box = p.start};
            p.ticks = 0;
            p.delta = {};
        }
    }
}

Carry Session::carry() const
{
    Carry next = m_start;
    next.lives = m_lives;
    next.tanks = m_tanks;
    next.weapons = m_hero.weapons;
    next.energy = m_hero.energy;
    next.weapon = m_hero.weapon;
    next.rematches = m_rematches;
    next.checkpoint = m_checkpoint;
    if (m_outcome == Outcome::Died) {
        next.lives = m_lives - 1;
    }
    return next;
}

float Session::doorOpen(const int door) const
{
    if (door < 0 || door >= static_cast<int>(m_doorOpen.size())) {
        return 0.0f;
    }
    return static_cast<float>(m_doorOpen[static_cast<std::size_t>(door)]) / static_cast<float>(kGateTicks);
}

float Session::scrollProgress() const
{
    return m_scroll ? m_scroll->progress() : 0.0f;
}

void Session::selectWeapon(const Weapon weapon)
{
    if (owns(weapon) && weapon != m_hero.weapon) {
        m_hero.weapon = weapon;
        m_hero.charge.reset();
        m_shots.removeIf([](const Shot& s) { return !s.extra.enemy; });
        m_events.push({.kind = EventKind::WeaponSwitch, .at = m_hero.body.box.center(), .weapon = weapon});
    }
}

bool Session::useTank()
{
    if (m_tanks <= 0 || m_hero.health.full() || m_phase == Phase::Dying || m_phase == Phase::Over) {
        return false;
    }
    --m_tanks;
    m_hero.health.refill();
    m_events.push({.kind = EventKind::Tank, .at = m_hero.body.box.center()});
    return true;
}

void Session::tick(const Input& input)
{
    m_hero.previous = m_hero.body.box;
    for (auto [handle, enemy] : m_enemies) {
        enemy.previous = enemy.body.box;
    }
    for (Pickup& p : m_pickups) {
        p.previous = p.body.box;
    }
    for (Platform& p : m_platforms) {
        p.previous = p.box;
    }
    for (Boss& b : m_bosses) {
        b.previous = b.body.box;
    }
    ++m_ticks;
    ++m_phaseTicks;
    switch (m_phase) {
    case Phase::Ready:
        if (m_phaseTicks >= kReadyTicks) {
            m_phase = Phase::Playing;
            m_phaseTicks = 0;
            m_events.push({.kind = EventKind::BeamIn, .at = m_hero.body.box.bottomCenter()});
        }
        break;
    case Phase::Playing: playTick(input); break;
    case Phase::Scroll: scrollTick(); break;
    case Phase::Gate: gateTick(); break;
    case Phase::BossIntro: {
        const int index = room().boss;
        if (index >= 0) {
            stepBoss(m_bosses[static_cast<std::size_t>(index)]);
        }
        // The hero lands and waits.
        (void)gamelib::core::stepPlatformer(m_hero.body, {}, heroTuning(false), tileWorld());
        break;
    }
    case Phase::BossDown:
        stepBosses();
        (void)gamelib::core::stepPlatformer(m_hero.body, {}, heroTuning(false), tileWorld());
        stepShots();
        if (m_phaseTicks == kBeamOutTicks) {
            m_events.push({.kind = EventKind::BeamOut, .at = m_hero.body.box.bottomCenter()});
        }
        if (m_phaseTicks >= kBossDownTicks) {
            m_events.push({.kind = EventKind::Clear, .at = m_hero.body.box.center()});
            m_outcome = Outcome::Cleared;
            m_phase = Phase::Over;
            m_phaseTicks = 0;
        }
        break;
    case Phase::Dying:
        if (m_phaseTicks >= kDeathTicks) {
            m_outcome = Outcome::Died;
            m_phase = Phase::Over;
            m_phaseTicks = 0;
        }
        break;
    case Phase::Over: break;
    }
}

void Session::playTick(const Input& input)
{
    stepPlatforms();
    stepHero(input);
    if (m_phase != Phase::Playing) {
        return;
    }
    checkRooms();
    if (m_phase != Phase::Playing) {
        return;
    }
    // A windy room gusts every few seconds.
    if (room().windy) {
        const int t = m_ticks % 300;
        m_wind = t >= 150 && t < 270 ? -0.55f : 0.0f;
    } else if (const Boss* boss = activeBoss(); boss == nullptr || boss->kind != BossKind::Gale) {
        m_wind = 0.0f;
    }
    Room& r = m_rooms[static_cast<std::size_t>(m_room)];
    for (const int point : r.spawner.tick(view().expanded(8.0f))) {
        spawnEnemy(point);
    }
    stepEnemies();
    stepBosses();
    stepShots();
    shotsHitEnemies();
    for (Boss& boss : m_bosses) {
        if (boss.room == m_room) {
            shotsHitBoss(boss);
        }
    }
    stepWarnings();
    heroMeetsEnemies();
    if (m_phase != Phase::Playing) {
        return;
    }
    stepPickups();
    checkCheckpoints();
    m_hero.vulnerable.tick();
    // Boss doors open when the hero walks into them (not while the room's boss lives).
    const Rect box = m_hero.body.box;
    const bool bossAlive = r.boss >= 0 && !m_bosses[static_cast<std::size_t>(r.boss)].sm.is(BossState::Dead);
    if (input.move > 0 && !bossAlive && !m_hero.body.climbing) {
        const gamelib::core::CellRange beside = gamelib::core::cellsBeside(box, 1, kTile);
        for (std::size_t d = 0; d < m_doors.size(); ++d) {
            for (const Cell c : m_doors[d].cells) {
                if (beside.contains(c)) {
                    startGate(static_cast<int>(d));
                    return;
                }
            }
        }
    }
}

void Session::checkRooms()
{
    const Rect box = m_hero.body.box;
    m_scroll = gamelib::core::RoomScroll::start(m_roomBounds, m_room, box, kScrollTicks, m_phase == Phase::Gate ? 20.0f : 12.0f);
    if (!m_scroll) {
        if (!room().bounds.contains(box.center()) && box.top() > room().bounds.bottom()) {
            die(); // fell out of the room with nothing below
        }
        return;
    }
    m_enemies = {};
    m_shots.clear();
    m_warnings.clear();
    m_events.push({.kind = EventKind::Scroll, .at = box.center()});
    if (m_phase == Phase::Gate) {
        m_gateStep = 2;
    } else {
        m_phase = Phase::Scroll;
    }
    m_phaseTicks = 0;
}

void Session::scrollTick()
{
    if (!m_scroll || !m_scroll->tick(m_hero.body.box)) {
        return;
    }
    const int to = m_scroll->to;
    m_scroll.reset();
    enterRoom(to);
    m_phaseTicks = 0;
    if (m_phase == Phase::Gate) {
        m_gateStep = 3;
        return;
    }
    m_phase = Phase::Playing;
    // Falling on keeps its speed; a jump up into the room above keeps a little.
    if (!m_hero.body.climbing && m_hero.body.velocity.y < 0.0f) {
        m_hero.body.velocity.y = std::min(m_hero.body.velocity.y, -2.0f);
    }
}
void Session::startGate(const int door)
{
    m_phase = Phase::Gate;
    m_phaseTicks = 0;
    m_gate = door;
    m_gateStep = 0;
    m_hero.body.sliding = false;
    m_hero.body.slideLeft = 0;
    m_hero.body.box = Rect::fromBottomCenter(m_hero.body.box.bottomCenter(), kHeroSize);
    m_hero.body.velocity = {};
    m_hero.charge.reset();
    m_shots.clear();
    m_events.push({.kind = EventKind::DoorOpen, .at = gamelib::core::cellRect(m_doors[static_cast<std::size_t>(door)].cells.front(), kTile).center()});
}

void Session::setDoor(const Door& door, const bool closed)
{
    for (const Cell c : door.cells) {
        m_map.layer(0).at(c) = closed ? 'D' : ' ';
    }
}

void Session::gateTick()
{
    const Door& door = m_doors[static_cast<std::size_t>(m_gate)];
    int& open = m_doorOpen[static_cast<std::size_t>(m_gate)];
    switch (m_gateStep) {
    case 0: // opening
        open = std::min(open + 1, kGateTicks);
        if (open >= kGateTicks) {
            setDoor(door, false);
            m_gateStep = 1;
            m_phaseTicks = 0;
        }
        break;
    case 1: { // walking through
        gamelib::core::PlatformerIntent intent{.move = 1.0f};
        (void)gamelib::core::stepPlatformer(m_hero.body, intent, heroTuning(false), tileWorld());
        m_hero.walked += kRunSpeed;
        checkRooms();
        if (m_phaseTicks > 120 && m_gateStep == 1) {
            m_gateStep = 3; // stuck: close again
        }
        break;
    }
    case 2: scrollTick(); break;
    case 3: // closing
        open = std::max(open - 1, 0);
        if (open == 0) {
            setDoor(door, true);
            m_events.push({.kind = EventKind::DoorClose, .at = gamelib::core::cellRect(door.cells.front(), kTile).center()});
            m_phase = Phase::Playing;
            m_phaseTicks = 0;
            m_gate = -1;
            const int boss = room().boss;
            if (boss >= 0 && m_bosses[static_cast<std::size_t>(boss)].sm.is(BossState::Waiting)) {
                startBossIntro(boss);
            }
        }
        break;
    default: break;
    }
}

void Session::unlockRoom(const int index)
{
    const Room& r = m_rooms[static_cast<std::size_t>(index)];
    gamelib::core::TileLayer& layer = m_map.layer(0);
    bool any = false;
    for (const Cell c : gamelib::core::cellsWith(layer, [](const gamelib::core::TileId id) { return id == 'L'; }, r.cells)) {
        layer.at(c) = ' ';
        any = true;
    }
    if (any) {
        m_events.push({.kind = EventKind::Unlock, .at = m_hero.body.box.center()});
    }
}

void Session::checkCheckpoints()
{
    for (std::size_t i = 0; i < m_checkpoints.size(); ++i) {
        const Checkpoint& c = m_checkpoints[i];
        if (static_cast<int>(i) > m_checkpoint && c.room == m_room && m_hero.body.box.center().x >= c.feet.x - kTile) {
            m_checkpoint = static_cast<int>(i);
            m_events.push({.kind = EventKind::Checkpoint, .at = c.feet});
        }
    }
}

void Session::stepPlatforms()
{
    const bool standing = m_hero.body.grounded;
    for (Platform& p : m_platforms) {
        if (p.room != m_room) {
            continue;
        }
        if (p.falling) {
            const bool ridden = standing && m_hero.body.ridingId == p.id;
            const gamelib::core::FallingPlatform::Step step =
                p.drop.step(ridden, {.delayTicks = 24, .acceleration = 0.18f, .maxSpeed = 5.0f});
            p.box = p.drop.box;
            p.delta = step.delta;
        } else {
            ++p.ticks;
            const Vec2 next = p.rail.at(static_cast<double>(kLiftSpeed) * p.ticks);
            p.delta = next - p.box.position();
            p.box.x = next.x;
            p.box.y = next.y;
        }
    }
}

void Session::stepPickups()
{
    const gamelib::core::TileWorld world = tileWorld();
    Hero& h = m_hero;
    std::erase_if(m_pickups, [&](Pickup& p) {
        if (p.room != m_room) {
            return false;
        }
        (void)gamelib::core::stepWalker(p.body, {.gravity = 0.2f, .maxFall = 4.0f}, world);
        if (p.life > 0 && --p.life == 0) {
            return true;
        }
        if (!gamelib::core::overlaps(p.body.box, h.body.box)) {
            return false;
        }
        switch (p.kind) {
        case PickupKind::SmallHealth: h.health.restore(3); break;
        case PickupKind::BigHealth: h.health.restore(10); break;
        case PickupKind::SmallEnergy:
        case PickupKind::BigEnergy: {
            const int amount = (p.kind == PickupKind::SmallEnergy ? 3 : 10) * kEnergyPerUnit;
            // The equipped weapon, or with the buster the emptiest one owned.
            Weapon target = h.weapon;
            if (target == Weapon::Buster) {
                int lowest = kMaxEnergy * kEnergyPerUnit;
                for (int w = 1; w < kWeaponCount; ++w) {
                    if (owns(static_cast<Weapon>(w)) && h.energy[static_cast<std::size_t>(w)].current < lowest) {
                        lowest = h.energy[static_cast<std::size_t>(w)].current;
                        target = static_cast<Weapon>(w);
                    }
                }
            }
            h.energy[static_cast<std::size_t>(target)].restore(amount);
            break;
        }
        case PickupKind::ExtraLife: m_lives = std::min(kMaxLives, m_lives + 1); break;
        case PickupKind::Tank: m_tanks = std::min(kMaxTanks, m_tanks + 1); break;
        }
        if (p.placed >= 0) {
            m_placedTaken[static_cast<std::size_t>(p.placed)] = true;
        }
        m_events.push({.kind = EventKind::Pickup, .at = p.body.box.center(), .pickup = p.kind});
        return true;
    });
}

void Session::dropPickup(const Vec2 at)
{
    const int roll = m_rng.range(0, 99);
    PickupKind kind = PickupKind::SmallHealth;
    if (roll < 22) {
        kind = PickupKind::SmallHealth;
    } else if (roll < 36) {
        kind = PickupKind::SmallEnergy;
    } else if (roll < 40) {
        kind = PickupKind::BigHealth;
    } else if (roll < 44) {
        kind = PickupKind::BigEnergy;
    } else if (roll < 45) {
        kind = PickupKind::ExtraLife;
    } else {
        return;
    }
    const Vec2 size = kind == PickupKind::BigHealth || kind == PickupKind::BigEnergy ? Vec2{14.0f, 14.0f} : Vec2{9.0f, 9.0f};
    const Rect box = Rect::fromCenter(at, size);
    m_pickups.push_back({.kind = kind, .body = {.box = box, .velocity = {0.0f, -2.0f}}, .previous = box, .room = m_room,
                         .life = kDropLifeTicks});
}

void Session::warn(const Vec2 at, const int ticks, const int kind)
{
    m_warnings.push_back({.at = at, .ticks = ticks, .kind = kind});
}

void Session::stepWarnings()
{
    const Rect r = room().bounds;
    std::erase_if(m_warnings, [&](Warning& w) {
        if (--w.ticks > 0) {
            return false;
        }
        Shot shot;
        shot.team = 1;
        shot.owner = 100;
        shot.tileContact = gamelib::core::TileContact::Ignore;
        shot.extra.enemy = true;
        switch (w.kind) {
        case 0: // lightning: a column from the ceiling to the floor
            shot.position = {w.at.x, r.top() + kViewHeight / 2.0f};
            shot.size = {12.0f, kViewHeight};
            shot.lifetimeTicks = 14;
            shot.damage = 4;
            shot.extra.look = 9;
            break;
        case 1: // a thorn bursting up from the floor
            shot.position = {w.at.x, w.at.y - 22.0f};
            shot.size = {12.0f, 44.0f};
            shot.lifetimeTicks = 26;
            shot.damage = 4;
            shot.extra.look = 10;
            break;
        case 2: // an icicle falling from the ceiling
        case 3: // debris
            shot.position = {w.at.x, r.top() + 2.0f * kTile + 8.0f};
            shot.size = {10.0f, 16.0f};
            shot.velocity = {0.0f, 1.0f};
            shot.gravity = 0.18f;
            shot.lifetimeTicks = 120;
            shot.damage = 3;
            shot.extra.look = w.kind == 2 ? 4 : 11;
            break;
        default: break;
        }
        m_shots.spawn(shot);
        m_events.push({.kind = EventKind::Strike, .at = w.at, .value = w.kind});
        return true;
    });
}

void Session::stepShots()
{
    // Homing wings and feathers turn towards their targets.
    const Vec2 heroAt = m_hero.body.box.center();
    for (Shot& s : m_shots.items()) {
        if (s.motion != gamelib::core::ProjectileMotion::Homing) {
            continue;
        }
        if (s.extra.enemy) {
            s.target = heroAt;
            continue;
        }
        float best = 1e9f;
        s.target = s.position + s.velocity * 40.0f;
        for (const auto [handle, e] : m_enemies) {
            const float d = gamelib::core::lengthSquared(e.body.box.center() - s.position);
            if (d < best) {
                best = d;
                s.target = e.body.box.center();
            }
        }
        if (const Boss* boss = activeBoss(); boss != nullptr && !boss->sm.is(BossState::Dead)) {
            const float d = gamelib::core::lengthSquared(boss->body.box.center() - s.position);
            if (d < best) {
                s.target = boss->body.box.center();
            }
        }
    }
    const gamelib::core::TileWorld world = tileWorld();
    m_shots.step(1.0f, gamelib::core::shotSolid(world), view().expanded(24.0f));
    for (const auto& removed : m_shots.removed()) {
        if (removed.projectile.extra.look == 1 && removed.reason == gamelib::core::ProjectileEnd::HitSolid) {
            // A lobbed bomb bursts where it lands.
            m_events.push({.kind = EventKind::Bomb, .at = removed.projectile.position});
            Shot blast;
            blast.position = removed.projectile.position;
            blast.size = {28.0f, 22.0f};
            blast.team = 1;
            blast.owner = 100;
            blast.damage = 3;
            blast.lifetimeTicks = 10;
            blast.tileContact = gamelib::core::TileContact::Ignore;
            blast.extra = {.enemy = true, .look = 12};
            m_shots.spawn(blast);
        }
    }
}

void Session::hurtHero(const int damage, const Rect& source)
{
    Hero& h = m_hero;
    h.health.restore(-damage);
    m_events.push({.kind = EventKind::Hurt, .at = h.body.box.center(), .value = damage});
    if (h.health.current <= 0) {
        die();
        return;
    }
    h.body.climbing = false;
    const Vec2 kb = gamelib::core::knockbackAway(source, h.body.box, kKnockbackSpeed, 0.0f, -h.body.facing);
    h.body.velocity.x = kb.x;
    if (h.body.velocity.y < 0.0f) {
        h.body.velocity.y = 0.0f;
    }
    h.body.lockTicks = kHurtStunTicks;
    h.charge.reset();
    h.hurtTicks = kHurtStunTicks;
}

void Session::die()
{
    if (m_phase == Phase::Dying || m_phase == Phase::Over) {
        return;
    }
    m_events.push({.kind = EventKind::Death, .at = m_hero.body.box.center()});
    m_phase = Phase::Dying;
    m_phaseTicks = 0;
    m_hero.health.current = 0;
    m_shots.clear();
    m_warnings.clear();
    m_wind = 0.0f;
}

std::uint64_t Session::hash() const
{
    gamelib::core::StateHash h;
    h.add(m_hero.body.box.x).add(m_hero.body.box.y).add(m_hero.body.velocity.x).add(m_hero.body.velocity.y);
    h.add(m_hero.health.current).add(m_room).add(static_cast<int>(m_phase)).add(m_ticks).add(m_lives);
    for (const auto [handle, e] : m_enemies) {
        h.add(static_cast<int>(e.kind)).add(e.body.box.x).add(e.body.box.y).add(e.hp);
    }
    for (const Boss& b : m_bosses) {
        h.add(b.hp).add(b.body.box.x).add(b.body.box.y);
    }
    return h.value();
}

} // namespace blaster::game
