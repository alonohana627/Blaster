// The twelve enemy kinds and the stage guardians (mini-bosses): spawning
// from the room's spawn points, their behaviour per tick, their shots, the
// hero's shots against them (shields deflect) and contact damage.
#include "session.hpp"

#include <gamelib/core/collision.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace blaster::game {

namespace {

// helpers

using gamelib::core::Rect;
using gamelib::core::Vec2;

struct KindInfo {
    Vec2 size;
    int hp = 1;
    int damage = 3;
};

constexpr std::array<KindInfo, kEnemyKinds> kKinds{{
    {{14.0f, 12.0f}, 2, 3}, // Crawler
    {{14.0f, 14.0f}, 3, 3}, // Hopper
    {{16.0f, 12.0f}, 2, 3}, // Diver
    {{16.0f, 20.0f}, 4, 4}, // ShieldWalker
    {{16.0f, 14.0f}, 3, 3}, // Turret
    {{22.0f, 22.0f}, 8, 3}, // Nest
    {{10.0f, 10.0f}, 1, 2}, // Drone
    {{14.0f, 14.0f}, 3, 4}, // Roller
    {{16.0f, 18.0f}, 4, 3}, // Lobber
    {{16.0f, 14.0f}, 3, 3}, // Popper
    {{14.0f, 12.0f}, 2, 3}, // Wisp
    {{14.0f, 14.0f}, 2, 3}, // Dropper
    {{36.0f, 36.0f}, 20, 4}, // MiniBoss
}};

const KindInfo& _info(const EnemyKind kind)
{
    return kKinds[static_cast<std::size_t>(kind)];
}

// What a hero shot takes from an ordinary enemy.
int _enemyDamage(const Weapon weapon, const int charge)
{
    switch (weapon) {
    case Weapon::Buster: return 1 + charge;
    case Weapon::Saw: return 2;
    case Weapon::Spark: return 2;
    case Weapon::Frost: return 1;
    case Weapon::Leaf: return 1;
    case Weapon::Magma: return 3;
    case Weapon::Gale: return 2;
    case Weapon::Count: break;
    }
    return 1;
}

int _sign(const float v)
{
    return v < 0.0f ? -1 : 1;
}

// The launch velocity that lands a lob of `gravity` on `to` after `ticks`.
Vec2 _lob(const Vec2 from, const Vec2 to, const float gravity, const int ticks)
{
    const auto t = static_cast<float>(ticks);
    const Vec2 d = to - from;
    const float vy = (d.y - gravity * t * (t + 1.0f) / 2.0f) / t;
    return {std::clamp(d.x / t, -3.0f, 3.0f), std::clamp(vy, -6.5f, 0.0f)};
}

} // namespace

void Session::spawnEnemy(const int point)
{
    Room& r = m_rooms[static_cast<std::size_t>(m_room)];
    const gamelib::core::SpawnPoint& sp = r.spawner.point(point);
    const auto kind = static_cast<EnemyKind>(sp.kind);
    if (kind == EnemyKind::MiniBoss && r.miniBossBeaten) {
        r.spawner.killed(point);
        return;
    }
    const KindInfo& info = _info(kind);
    Enemy e;
    e.kind = kind;
    e.point = point;
    e.hp = e.maxHp = info.hp;
    e.origin = sp.position;
    e.variant = kind == EnemyKind::MiniBoss ? def().miniBoss : 0;
    const Vec2 cellCenter = sp.position;
    const Vec2 feet = cellCenter + Vec2{0.0f, kTile / 2.0f};
    switch (kind) {
    case EnemyKind::Diver:
    case EnemyKind::Drone:
    case EnemyKind::Wisp:
    case EnemyKind::Nest: e.body.box = Rect::fromCenter(cellCenter, info.size); break;
    case EnemyKind::Dropper:
        e.body.box = Rect{cellCenter.x - info.size.x / 2.0f, cellCenter.y - kTile / 2.0f, info.size.x, info.size.y};
        break;
    case EnemyKind::MiniBoss: e.body.box = Rect::fromBottomCenter(sp.position, info.size); break;
    default: e.body.box = Rect::fromBottomCenter(feet, info.size); break;
    }
    // Turrets hang from a ceiling when there is one right above them.
    if (kind == EnemyKind::Turret) {
        const gamelib::core::Cell cell = gamelib::core::cellAt(cellCenter, kTile);
        if (tileWorld().shapeAt({cell.x, cell.y - 1}) == gamelib::core::TileShape::Solid) {
            e.variant = 1;
            e.body.box = Rect{cellCenter.x - info.size.x / 2.0f, cellCenter.y - kTile / 2.0f, info.size.x, info.size.y};
        }
    }
    e.body.direction = m_hero.body.box.center().x < e.body.box.center().x ? -1 : 1;
    e.previous = e.body.box;
    e.vulnerable = gamelib::core::Vulnerable{e.body.box, 1, kind == EnemyKind::MiniBoss ? 8 : 0, 0};
    const bool shielded = kind == EnemyKind::ShieldWalker || kind == EnemyKind::Turret || kind == EnemyKind::Popper;
    e.vulnerable.setGuarded(shielded);
    m_enemies.insert(e);
}

void Session::enemyFire(const Vec2 at, const Vec2 velocity, const int look, const int damage)
{
    Shot s;
    s.position = at;
    s.velocity = velocity;
    s.size = look == 2 ? Vec2{10.0f, 10.0f} : Vec2{7.0f, 7.0f};
    s.team = 1;
    s.owner = 100;
    s.damage = damage;
    s.lifetimeTicks = 300;
    s.tileContact = gamelib::core::TileContact::Ignore;
    s.extra.enemy = true;
    s.extra.look = look;
    if (m_shots.spawn(s) != 0) {
        m_events.push({.kind = EventKind::EnemyShot, .at = at});
    }
}

void Session::stepEnemies()
{
    for (auto [handle, e] : m_enemies) {
        stepEnemy(e);
    }
    // Enemies that left the screen go back to their spawn points.
    const Rect keep = view().expanded(40.0f);
    Room& r = m_rooms[static_cast<std::size_t>(m_room)];
    m_enemies.eraseIf([&](gamelib::core::Handle<Enemy>, const Enemy& e) {
        if (e.kind == EnemyKind::MiniBoss || gamelib::core::overlaps(e.body.box, keep)) {
            return false;
        }
        if (e.point >= 0) {
            r.spawner.despawned(e.point);
        }
        return true;
    });
}

void Session::stepEnemy(Enemy& e)
{
    const gamelib::core::TileWorld world = tileWorld();
    const Vec2 hero = m_hero.body.box.center();
    const Vec2 me = e.body.box.center();
    const float dx = hero.x - me.x;
    const float dy = hero.y - me.y;
    e.flash = std::max(0, e.flash - 1);
    e.vulnerable.tick();
    if (e.frozen > 0) {
        --e.frozen;
        e.vulnerable.setHurtbox(e.body.box);
        return;
    }
    ++e.age;
    ++e.timer;
    const gamelib::core::WalkerMotion fall{.gravity = 0.25f, .maxFall = 5.0f};
    switch (e.kind) {
    case EnemyKind::Crawler:
        (void)gamelib::core::stepWalker(e.body, {.gravity = 0.25f, .maxFall = 5.0f, .speed = 0.55f, .turnAtLedges = true},
                                        world);
        break;
    case EnemyKind::Hopper: {
        if (e.body.grounded) {
            e.body.velocity.x = 0.0f;
            if (e.timer >= 64) {
                e.body.direction = _sign(dx);
                e.body.velocity = {0.9f * static_cast<float>(e.body.direction), -3.9f};
                e.timer = 0;
            }
        }
        (void)gamelib::core::stepWalker(e.body, {.gravity = 0.22f, .maxFall = 5.0f}, world);
        break;
    }
    case EnemyKind::Diver: {
        switch (e.state) {
        case 0: { // hover, bobbing
            const float y = e.origin.y + 4.0f * std::sin(static_cast<float>(e.age) * 0.08f);
            e.body.box.y = y - e.body.box.height / 2.0f;
            if (std::abs(dx) < 84.0f && dy > 8.0f && e.timer > 40) {
                e.state = 1;
                e.timer = 0;
                e.body.velocity = gamelib::core::normalized(hero - me) * 2.6f;
                e.body.direction = _sign(dx);
            }
            break;
        }
        case 1: { // dive
            const gamelib::core::BoxMove move = gamelib::core::moveBox(e.body.box, e.body.velocity, world);
            e.body.box = move.box;
            if (move.contacts.blockedX || move.contacts.blockedY || e.timer > 70) {
                e.state = 2;
                e.timer = 0;
            }
            break;
        }
        default: { // back up to its post
            const Vec2 home = e.origin - e.body.box.size() / 2.0f;
            const Vec2 p = gamelib::core::approach(e.body.box.position(), home, 1.2f);
            e.body.box.x = p.x;
            e.body.box.y = p.y;
            if (p == home) {
                e.state = 0;
                e.timer = 0;
            }
            break;
        }
        }
        break;
    }
    case EnemyKind::ShieldWalker: {
        if (e.state == 0) {
            if (e.timer % 40 == 1 && e.body.grounded) {
                e.body.direction = _sign(dx);
            }
            (void)gamelib::core::stepWalker(e.body, {.gravity = 0.25f, .maxFall = 5.0f, .speed = 0.4f, .turnAtLedges = true},
                                            world);
            if (e.timer >= 130 && std::abs(dx) < 220.0f) {
                e.state = 1;
                e.timer = 0;
                e.body.direction = _sign(dx);
                e.vulnerable.setGuarded(false);
            }
        } else {
            (void)gamelib::core::stepWalker(e.body, fall, world);
            if (e.timer == 18) {
                enemyFire(me + Vec2{static_cast<float>(e.body.direction) * 10.0f, -2.0f},
                          {static_cast<float>(e.body.direction) * 2.6f, 0.0f}, 0, 3);
            }
            if (e.timer >= 52) {
                e.state = 0;
                e.timer = 0;
                e.vulnerable.setGuarded(true);
            }
        }
        break;
    }
    case EnemyKind::Turret: {
        e.body.direction = _sign(dx);
        if (e.state == 0) {
            if (e.timer >= 80 && std::abs(dx) < 210.0f) {
                e.state = 1;
                e.timer = 0;
                e.vulnerable.setGuarded(false);
            }
        } else {
            if (e.timer == 16 || e.timer == 40) {
                enemyFire(me, gamelib::core::normalized(hero - me) * 2.2f, 0, 3);
            }
            if (e.timer >= 64) {
                e.state = 0;
                e.timer = 0;
                e.vulnerable.setGuarded(true);
            }
        }
        break;
    }
    case EnemyKind::Nest: {
        int drones = 0;
        for (const auto [h, other] : m_enemies) {
            drones += other.kind == EnemyKind::Drone && other.point < 0 ? 1 : 0;
        }
        if (e.timer >= 150 && drones < 3 && std::abs(dx) < 240.0f) {
            e.timer = 0;
            Enemy d;
            d.kind = EnemyKind::Drone;
            d.hp = d.maxHp = _info(EnemyKind::Drone).hp;
            d.body.box = Rect::fromCenter(me, _info(EnemyKind::Drone).size);
            d.previous = d.body.box;
            d.origin = me;
            d.body.velocity = {0.0f, -1.0f};
            d.vulnerable = gamelib::core::Vulnerable{d.body.box, 1};
            m_enemies.insert(d);
            m_events.push({.kind = EventKind::EnemyShot, .at = me});
        }
        break;
    }
    case EnemyKind::Drone: {
        const Vec2 wanted = gamelib::core::normalized(hero - me) * 0.95f;
        e.body.velocity = gamelib::core::approach(e.body.velocity, wanted, 0.045f);
        e.body.box.x += e.body.velocity.x;
        e.body.box.y += e.body.velocity.y;
        e.body.direction = _sign(e.body.velocity.x);
        break;
    }
    case EnemyKind::Roller: {
        if (e.state == 0) {
            (void)gamelib::core::stepWalker(e.body, fall, world);
            if (std::abs(dx) < 140.0f && std::abs(dy) < 48.0f && e.timer > 20) {
                e.state = 1;
                e.body.direction = _sign(dx);
            }
        } else {
            (void)gamelib::core::stepWalker(e.body, {.gravity = 0.25f, .maxFall = 5.0f, .speed = 2.0f}, world);
        }
        break;
    }
    case EnemyKind::Lobber: {
        e.body.direction = _sign(dx);
        (void)gamelib::core::stepWalker(e.body, fall, world);
        if (e.timer >= 100 && std::abs(dx) < 230.0f) {
            e.timer = 0;
            constexpr float kGravity = 0.2f;
            const Vec2 from = me + Vec2{0.0f, -10.0f};
            Shot s;
            s.position = from;
            s.velocity = _lob(from, m_hero.body.box.bottomCenter() + Vec2{0.0f, -6.0f}, kGravity, 48);
            s.gravity = kGravity;
            s.size = {10.0f, 10.0f};
            s.team = 1;
            s.owner = 100;
            s.damage = 3;
            s.lifetimeTicks = 240;
            s.tileContact = gamelib::core::TileContact::Remove;
            s.extra = {.enemy = true, .look = 1};
            if (m_shots.spawn(s) != 0) {
                m_events.push({.kind = EventKind::EnemyShot, .at = from});
            }
        }
        break;
    }
    case EnemyKind::Popper: {
        e.body.direction = _sign(dx);
        (void)gamelib::core::stepWalker(e.body, fall, world);
        if (e.state == 0) {
            if (e.timer >= 90 && std::abs(dx) < 180.0f) {
                e.state = 1;
                e.timer = 0;
                e.vulnerable.setGuarded(false);
            }
        } else {
            if (e.timer == 14) {
                const float base = std::atan2(dy, dx);
                for (const float spread : {-0.42f, 0.0f, 0.42f}) {
                    enemyFire(me + Vec2{0.0f, -4.0f}, gamelib::core::fromAngle(base + spread) * 2.0f, 0, 3);
                }
            }
            if (e.timer >= 56) {
                e.state = 0;
                e.timer = 0;
                e.vulnerable.setGuarded(true);
            }
        }
        break;
    }
    case EnemyKind::Wisp: {
        if (e.age == 1) {
            e.body.direction = _sign(dx);
        }
        e.body.box.x += static_cast<float>(e.body.direction) * 1.1f;
        const float y = e.origin.y + 22.0f * std::sin(static_cast<float>(e.age) * 2.0f * gamelib::core::kPi / 80.0f);
        e.body.box.y = y - e.body.box.height / 2.0f;
        break;
    }
    case EnemyKind::Dropper: {
        if (e.state == 0) {
            if (std::abs(dx) < 26.0f && dy > 0.0f) {
                e.state = 1;
                e.timer = 0;
            }
        } else {
            if (e.body.grounded) {
                e.body.velocity.x = 0.0f;
                if (e.timer >= 46) {
                    e.body.direction = _sign(dx);
                    e.body.velocity = {0.8f * static_cast<float>(e.body.direction), -3.2f};
                    e.timer = 0;
                }
            }
            (void)gamelib::core::stepWalker(e.body, {.gravity = 0.3f, .maxFall = 6.0f}, world);
        }
        break;
    }
    case EnemyKind::MiniBoss: {
        // A cycle of hop, spread and charge, faster at half health.
        const bool angry = e.hp * 2 <= e.maxHp;
        const int idle = angry ? 34 : 50;
        if (e.body.grounded && e.state != 3) {
            e.body.velocity.x = 0.0f;
        }
        switch (e.state) {
        case 0: // idle, facing the hero
            e.body.direction = _sign(dx);
            if (e.timer >= idle) {
                e.shots = gamelib::core::nextPattern(m_rng, 3, e.shots);
                e.state = 1 + e.shots;
                e.timer = 0;
            }
            break;
        case 1: // a hop at the hero
            if (e.timer == 14) {
                const float vx = std::clamp(dx / 48.0f, -1.8f, 1.8f);
                e.body.velocity = {vx, -5.0f};
                e.body.grounded = false;
            }
            if (e.timer > 16 && e.body.grounded) {
                m_events.push({.kind = EventKind::BossLand, .at = e.body.box.bottomCenter()});
                e.state = 0;
                e.timer = 0;
            }
            break;
        case 2: // a spread of shots
            if (e.timer == 22 || (angry && e.timer == 46)) {
                const float base = std::atan2(dy, dx);
                const int count = e.variant >= 3 ? 5 : 3;
                for (int i = 0; i < count; ++i) {
                    const float a = base + (static_cast<float>(i) - static_cast<float>(count - 1) / 2.0f) * 0.3f;
                    enemyFire(me, gamelib::core::fromAngle(a) * 2.3f, 2, 4);
                }
            }
            if (e.timer >= (angry ? 64 : 40)) {
                e.state = 0;
                e.timer = 0;
            }
            break;
        default: // a charge along the floor
            if (e.timer == 1) {
                e.body.direction = _sign(dx);
            }
            if (e.timer > 26) {
                e.body.velocity.x = static_cast<float>(e.body.direction) * (angry ? 2.8f : 2.3f);
            }
            break;
        }
        const gamelib::core::WalkerStep step = gamelib::core::stepWalker(e.body, {.gravity = 0.25f, .maxFall = 6.0f}, world);
        if (e.state == 3 && (step.contacts.blockedX || e.timer > 160)) {
            e.body.velocity.x = 0.0f;
            e.state = 0;
            e.timer = 0;
            m_events.push({.kind = EventKind::BossLand, .at = e.body.box.bottomCenter()});
        }
        break;
    }
    case EnemyKind::Count: break;
    }
    e.vulnerable.setHurtbox(e.body.box);
}

void Session::shotsHitEnemies()
{
    for (auto [handle, e] : m_enemies) {
        if (e.hp <= 0) {
            continue;
        }
        if (e.vulnerable.guarded()) {
            const Rect box = e.body.box;
            m_shots.removeIf([&](const Shot& s) {
                if (s.extra.enemy || !gamelib::core::overlaps(s.box(), box)) {
                    return false;
                }
                m_events.push({.kind = EventKind::Deflect, .at = s.position, .value = s.velocity.x < 0.0f ? -1 : 1,
                               .weapon = s.extra.weapon});
                return true;
            });
            continue;
        }
        while (const auto hit = m_shots.hitTest(e.vulnerable)) {
            const ShotData& data = hit->projectile.extra;
            e.hp -= _enemyDamage(data.weapon, data.charge);
            e.flash = 6;
            if (data.weapon == Weapon::Frost && e.kind != EnemyKind::MiniBoss) {
                e.frozen = 150;
            }
            m_events.push({.kind = EventKind::HitEnemy, .at = hit->projectile.position, .weapon = data.weapon,
                           .enemy = e.kind});
            if (e.hp <= 0) {
                enemyDown(e);
                break;
            }
        }
    }
    m_enemies.eraseIf([](gamelib::core::Handle<Enemy>, const Enemy& e) { return e.hp <= 0; });
}

void Session::enemyDown(Enemy& e)
{
    const Vec2 at = e.body.box.center();
    m_events.push({.kind = EventKind::EnemyDown, .at = at, .value = e.kind == EnemyKind::MiniBoss ? 1 : 0, .enemy = e.kind});
    ++m_destroyed;
    Room& r = m_rooms[static_cast<std::size_t>(m_room)];
    if (e.point >= 0) {
        r.spawner.killed(e.point);
    }
    if (e.kind == EnemyKind::MiniBoss) {
        r.miniBossBeaten = true;
        unlockRoom(m_room);
        if (m_room == static_cast<int>(m_rooms.size()) - 1) {
            m_phase = Phase::BossDown;
            m_phaseTicks = 0;
            m_shots.clear();
            return;
        }
        // A guardian always leaves something useful.
        const Rect box = Rect::fromCenter(at, {14.0f, 14.0f});
        m_pickups.push_back({.kind = m_hero.health.current < kMaxHealth / 2 ? PickupKind::BigHealth : PickupKind::BigEnergy,
                             .body = {.box = box, .velocity = {0.0f, -2.0f}}, .previous = box, .room = m_room,
                             .life = 900});
        return;
    }
    if (e.kind != EnemyKind::Drone || m_rng.chance(0.3f)) {
        dropPickup(at);
    }
}

void Session::heroMeetsEnemies()
{
    Hero& h = m_hero;
    for (const auto [handle, e] : m_enemies) {
        if (e.frozen > 0 || m_phase != Phase::Playing) {
            continue;
        }
        const gamelib::core::Hit hit{.box = e.body.box.expanded(-2.0f), .damage = _info(e.kind).damage, .team = 1};
        if (const auto landed = h.vulnerable.tryHit(hit)) {
            hurtHero(landed->damage, e.body.box);
        }
    }
    if (const Boss* boss = activeBoss(); boss != nullptr && m_phase == Phase::Playing) {
        const bool fighting = !boss->sm.is(BossState::Dead) && !boss->sm.is(BossState::Intro)
                              && !boss->sm.is(BossState::Waiting);
        if (fighting) {
            const gamelib::core::Hit hit{.box = boss->body.box.expanded(-3.0f), .damage = 4, .team = 1};
            if (const auto landed = h.vulnerable.tryHit(hit)) {
                hurtHero(landed->damage, boss->body.box);
            }
        }
    }
    while (m_phase == Phase::Playing) {
        const auto hit = m_shots.hitTest(h.vulnerable);
        if (!hit) {
            break;
        }
        hurtHero(hit->landed.damage, hit->projectile.box());
    }
}

} // namespace blaster::game
