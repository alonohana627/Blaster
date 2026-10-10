// The six robot masters and the Overseer: their intro (falling in, a pose,
// the health bar filling), a state machine per boss with two or three
// patterns chosen without repeating, new patterns below half and a quarter
// of their health (core::PhaseByHealth), and their defeat.
#include "session.hpp"

#include <gamelib/ai/opponent.hpp>
#include <gamelib/core/collision.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace blaster::game {

namespace {

// helpers

using gamelib::core::Rect;
using gamelib::core::Vec2;

constexpr float kBossGravity = 0.25f;
constexpr int kPhaseTicks = 40; // the pause (and invincibility) when a boss changes its pattern set

Vec2 _bossSize(const BossKind kind)
{
    switch (kind) {
    case BossKind::Grinder: return {28.0f, 28.0f};
    case BossKind::Storm: return {26.0f, 30.0f};
    case BossKind::Rime: return {24.0f, 26.0f};
    case BossKind::Thorn: return {30.0f, 32.0f};
    case BossKind::Cinder: return {28.0f, 28.0f};
    case BossKind::Gale: return {30.0f, 24.0f};
    case BossKind::Overseer: return {56.0f, 64.0f};
    case BossKind::Count: break;
    }
    return {28.0f, 28.0f};
}

bool _flies(const BossKind kind)
{
    return kind == BossKind::Storm || kind == BossKind::Gale || kind == BossKind::Overseer;
}

int _sign(const float v)
{
    return v < 0.0f ? -1 : 1;
}

// Where a flier hovers: its marker raised by a few tiles.
float _hoverY(const Boss& b)
{
    switch (b.kind) {
    case BossKind::Storm: return b.home.y - 2.5f * kTile; // low enough for a jumping shot
    case BossKind::Gale: return b.home.y - 4.0f * kTile;
    case BossKind::Overseer: return b.home.y - 1.5f * kTile;
    default: return b.home.y;
    }
}


} // namespace

void Session::startBossIntro(const int index)
{
    Boss& b = m_bosses[static_cast<std::size_t>(index)];
    const Room& r = m_rooms[static_cast<std::size_t>(b.room)];
    const Vec2 size = _bossSize(b.kind);
    b.flying = _flies(b.kind);
    b.body = {};
    b.body.box = Rect::fromBottomCenter({b.home.x, r.bounds.top()}, size);
    b.previous = b.body.box;
    b.body.direction = m_hero.body.box.center().x < b.home.x ? -1 : 1;
    b.hp = kBossHealth;
    b.shown = 0;
    b.pattern = -1;
    b.phases.reset();
    b.sm.enter(BossState::Intro);
    m_phase = Phase::BossIntro;
    m_phaseTicks = 0;
    m_hero.charge.reset();
    m_events.push({.kind = EventKind::BossIntro, .at = b.home, .boss = b.kind});
}

void Session::stepBosses()
{
    for (Boss& b : m_bosses) {
        if (b.room == m_room && !b.sm.is(BossState::Waiting)) {
            stepBoss(b);
        }
    }
}

void Session::bossFire(const Boss& boss, const Vec2 at, const Vec2 velocity, const int look, const int damage,
                       const float gravity)
{
    Shot s;
    s.position = at;
    s.velocity = velocity;
    s.size = look == 13 ? Vec2{14.0f, 14.0f} : (look == 6 ? Vec2{12.0f, 14.0f} : Vec2{10.0f, 10.0f});
    s.team = 1;
    s.owner = 200 + static_cast<int>(boss.kind);
    s.damage = damage;
    s.gravity = gravity;
    s.lifetimeTicks = 300;
    s.tileContact = gravity > 0.0f && look != 13 ? gamelib::core::TileContact::Remove : gamelib::core::TileContact::Ignore;
    s.extra = {.enemy = true, .look = look};
    if (m_shots.spawn(s) != 0) {
        m_events.push({.kind = EventKind::EnemyShot, .at = at, .boss = boss.kind});
    }
}

void Session::stepBoss(Boss& b)
{
    b.flash = std::max(0, b.flash - 1);
    b.vulnerable.tick();
    const Room& r = m_rooms[static_cast<std::size_t>(b.room)];
    if (b.sm.is(BossState::Dead)) {
        b.sm.tick();
        return;
    }
    if (b.sm.is(BossState::Intro)) {
        // Fall (or float) in, pose, then fill the health bar.
        const float targetBottom = b.flying ? _hoverY(b) : b.home.y;
        if (b.body.box.bottom() < targetBottom) {
            b.body.box.y = std::min(b.body.box.y + (b.flying ? 2.5f : 4.0f), targetBottom - b.body.box.height);
            if (b.body.box.bottom() >= targetBottom && !b.flying) {
                m_events.push({.kind = EventKind::BossLand, .at = b.body.box.bottomCenter()});
            }
            b.sm.enter(BossState::Intro); // the pose starts on landing
        } else if (b.sm.ticksIn() >= kBossIntroTicks && b.sm.ticksIn() % kBossFillTicks == 0) {
            if (b.shown < kBossHealth) {
                ++b.shown;
                m_events.push({.kind = EventKind::BossFill, .at = b.body.box.center(), .value = b.shown});
            } else {
                b.sm.enter(BossState::Idle);
                m_phase = Phase::Playing;
                m_phaseTicks = 0;
            }
        }
        b.sm.tick();
        b.vulnerable.setHurtbox(b.body.box);
        return;
    }
    b.shown = b.hp;
    switch (b.kind) {
    case BossKind::Grinder: stepGrinder(b); break;
    case BossKind::Storm: stepStorm(b); break;
    case BossKind::Rime: stepRime(b); break;
    case BossKind::Thorn: stepThorn(b); break;
    case BossKind::Cinder: stepCinder(b); break;
    case BossKind::Gale: stepGale(b); break;
    case BossKind::Overseer: stepOverseer(b); break;
    case BossKind::Count: break;
    }
    // Never leave the room.
    const Rect inner = r.bounds.expanded(-kTile);
    b.body.box.x = std::clamp(b.body.box.x, inner.left(), inner.right() - b.body.box.width);
    b.body.box.y = std::clamp(b.body.box.y, inner.top(), inner.bottom() - b.body.box.height);
    b.sm.tick();
    b.vulnerable.setHurtbox(b.body.box.expanded(-1.0f));
}

void Session::shotsHitBoss(Boss& b)
{
    if (b.sm.is(BossState::Dead) || b.sm.is(BossState::Intro) || b.sm.is(BossState::Waiting)) {
        return;
    }
    while (const auto hit = m_shots.hitTest(b.vulnerable)) {
        const ShotData& data = hit->projectile.extra;
        const int damage = bossDamage(b.kind, data.weapon, data.charge);
        b.hp -= damage;
        b.flash = 12;
        m_events.push({.kind = EventKind::BossHit, .at = hit->projectile.position, .value = std::max(b.hp, 0),
                       .weapon = data.weapon, .boss = b.kind});
        if (b.hp <= 0) {
            bossDefeated(b);
            return;
        }
        if (b.phases.update(b.hp, kBossHealth)) {
            m_events.push({.kind = EventKind::BossPhase, .at = b.body.box.center(), .value = b.phases.phase(), .boss = b.kind});
            b.vulnerable.makeInvincible(kPhaseTicks);
            b.sm.enter(BossState::Recover);
            b.count = 0;
            m_wind = 0.0f;
        }
    }
}

void Session::bossDefeated(Boss& b)
{
    b.hp = 0;
    b.shown = 0;
    b.sm.enter(BossState::Dead);
    m_events.push({.kind = EventKind::BossDown, .at = b.body.box.center(), .boss = b.kind});
    m_shots.clear();
    m_warnings.clear();
    m_wind = 0.0f;
    m_rematches = static_cast<std::uint8_t>(m_rematches | (1u << static_cast<int>(b.kind)));
    m_enemies = {};
    if (b.room == static_cast<int>(m_rooms.size()) - 1) {
        m_phase = Phase::BossDown;
        m_phaseTicks = 0;
        m_hero.charge.reset();
        return;
    }
    // A rematch in the fortress: a refill for the next one.
    const Rect box = Rect::fromCenter(b.body.box.center(), {14.0f, 14.0f});
    m_pickups.push_back({.kind = PickupKind::BigHealth, .body = {.box = box, .velocity = {0.0f, -2.0f}}, .previous = box,
                         .room = m_room, .life = -1});
}

// --- GEARGRINDER: saws along the floor, hops, and (below a quarter) a dash.
void Session::stepGrinder(Boss& b)
{
    const gamelib::core::TileWorld world = tileWorld();
    const Vec2 hero = m_hero.body.box.center();
    const Vec2 me = b.body.box.center();
    const int phase = b.phases.phase();
    const float fast = 1.0f + 0.18f * static_cast<float>(phase);
    if (b.body.grounded && !b.sm.is(BossState::Attack2)) {
        b.body.velocity.x = 0.0f;
    }
    switch (b.sm.current()) {
    case BossState::Idle:
        b.body.direction = _sign(hero.x - me.x);
        if (b.sm.after(40 - 8 * phase)) {
            b.pattern = gamelib::core::nextPattern(m_rng, phase >= 2 ? 3 : 2, b.pattern);
            b.target = hero;
            b.sm.enter(b.pattern == 0 ? BossState::Attack : (b.pattern == 1 ? BossState::Jump : BossState::Attack2));
        }
        break;
    case BossState::Attack: // raise the saw, throw it along the floor
        if (b.sm.ticksIn() == 24) {
            const auto dir = static_cast<float>(b.body.direction);
            bossFire(b, {me.x + dir * 16.0f, b.home.y - 7.0f}, {dir * 2.0f * fast, 0.0f}, 13, 4);
            if (phase >= 1) {
                bossFire(b, me + Vec2{dir * 12.0f, -10.0f}, {dir * 2.3f, -3.6f}, 13, 4, 0.14f);
            }
        }
        if (b.sm.after(52)) {
            b.sm.enter(BossState::Idle);
        }
        break;
    case BossState::Jump:
        if (b.sm.ticksIn() == 18) {
            b.body.velocity = {std::clamp((b.target.x - me.x) / 44.0f, -2.6f, 2.6f), -5.5f};
            b.body.grounded = false;
        }
        if (b.sm.ticksIn() > 20 && b.body.grounded) {
            m_events.push({.kind = EventKind::BossLand, .at = b.body.box.bottomCenter()});
            b.sm.enter(BossState::Idle);
        }
        break;
    case BossState::Attack2: // sparks, then a dash to the wall
        if (b.sm.ticksIn() < 30) {
            b.body.velocity.x = 0.0f;
        } else {
            b.body.velocity.x = static_cast<float>(b.body.direction) * 3.4f;
        }
        break;
    case BossState::Recover:
        b.body.velocity.x = 0.0f;
        if (b.sm.after(kPhaseTicks)) {
            b.sm.enter(BossState::Idle);
        }
        break;
    default: b.sm.enter(BossState::Idle); break;
    }
    const gamelib::core::WalkerStep step =
        gamelib::core::stepWalker(b.body, {.gravity = kBossGravity, .maxFall = 6.0f}, world);
    if (b.sm.is(BossState::Attack2) && b.sm.ticksIn() >= 30 && (step.contacts.blockedX || b.sm.after(150))) {
        b.body.velocity.x = 0.0f;
        m_events.push({.kind = EventKind::BossLand, .at = b.body.box.bottomCenter()});
        b.sm.enter(BossState::Recover);
    }
}

// --- STORMCALLER: floats, fans of sparks, lightning at the hero, crossing over.
void Session::stepStorm(Boss& b)
{
    const Room& r = room();
    const Vec2 hero = m_hero.body.box.center();
    const Vec2 me = b.body.box.center();
    const int phase = b.phases.phase();
    const float fast = 1.0f + 0.2f * static_cast<float>(phase);
    const float hoverY = _hoverY(b) - b.body.box.height + 6.0f * std::sin(static_cast<float>(m_ticks) * 0.06f);
    b.body.box.y = gamelib::core::approach(b.body.box.y, hoverY, 1.2f);
    switch (b.sm.current()) {
    case BossState::Idle:
        b.body.direction = _sign(hero.x - me.x);
        if (b.sm.after(36 - 8 * phase)) {
            b.pattern = gamelib::core::nextPattern(m_rng, 3, b.pattern);
            b.sm.enter(b.pattern == 0 ? BossState::Attack : (b.pattern == 1 ? BossState::Attack2 : BossState::Move));
            // Cross to the side away from where it is.
            const float left = r.bounds.left() + 3.0f * kTile;
            const float right = r.bounds.right() - 3.0f * kTile;
            b.target = {me.x < r.bounds.center().x ? right : left, me.y};
        }
        break;
    case BossState::Attack: // a fan of sparks at the hero
        if (b.sm.ticksIn() == 22) {
            const float base = std::atan2(hero.y - me.y, hero.x - me.x);
            const int count = phase >= 1 ? 5 : 3;
            for (int i = 0; i < count; ++i) {
                const float a = base + (static_cast<float>(i) - static_cast<float>(count - 1) / 2.0f) * 0.32f;
                bossFire(b, me, gamelib::core::fromAngle(a) * 2.2f * fast, 3, 3);
            }
        }
        if (b.sm.after(44)) {
            b.sm.enter(BossState::Idle);
        }
        break;
    case BossState::Attack2: // lightning where the hero stands (and beside him later)
        if (b.sm.ticksIn() == 4) {
            warn({hero.x, b.home.y}, 52, 0);
            if (phase >= 1) {
                warn({hero.x - 72.0f, b.home.y}, 52, 0);
                warn({hero.x + 72.0f, b.home.y}, 52, 0);
            }
        }
        if (b.sm.after(70)) {
            b.sm.enter(BossState::Idle);
        }
        break;
    case BossState::Move: {
        const float x = gamelib::core::approach(me.x, b.target.x, 1.8f * fast);
        b.body.box.x = x - b.body.box.width / 2.0f;
        b.body.direction = _sign(b.target.x - me.x);
        if (x == b.target.x || b.sm.after(150)) {
            b.sm.enter(BossState::Idle);
        }
        break;
    }
    case BossState::Recover:
        if (b.sm.after(kPhaseTicks)) {
            b.sm.enter(BossState::Idle);
        }
        break;
    default: b.sm.enter(BossState::Idle); break;
    }
}

// --- RIMEFANG: skates across the floor, rains icicles, throws ice shards.
void Session::stepRime(Boss& b)
{
    const gamelib::core::TileWorld world = tileWorld();
    const Vec2 hero = m_hero.body.box.center();
    const Vec2 me = b.body.box.center();
    const int phase = b.phases.phase();
    const float fast = 1.0f + 0.18f * static_cast<float>(phase);
    if (b.body.grounded && !b.sm.is(BossState::Attack)) {
        b.body.velocity.x = 0.0f;
    }
    switch (b.sm.current()) {
    case BossState::Idle:
        b.body.direction = _sign(hero.x - me.x);
        if (b.sm.after(40 - 8 * phase)) {
            b.pattern = gamelib::core::nextPattern(m_rng, 3, b.pattern);
            b.sm.enter(b.pattern == 0 ? BossState::Attack : (b.pattern == 1 ? BossState::Attack2 : BossState::Attack3));
        }
        break;
    case BossState::Attack: // crouch, then skate to the wall
        b.body.velocity.x = b.sm.ticksIn() < 22 ? 0.0f : static_cast<float>(b.body.direction) * 3.0f * fast;
        break;
    case BossState::Attack2: // a hop, and icicles fall around the hero
        if (b.sm.ticksIn() == 0) {
            b.body.velocity = {0.0f, -4.5f};
            b.body.grounded = false;
        }
        if (b.sm.ticksIn() == 12) {
            const int count = 3 + phase;
            for (int i = 0; i < count; ++i) {
                const float offset = (static_cast<float>(i) - static_cast<float>(count - 1) / 2.0f) * 44.0f;
                warn({hero.x + offset, b.home.y}, 40 + 6 * i, 2);
            }
        }
        if (b.sm.after(76)) {
            b.sm.enter(BossState::Idle);
        }
        break;
    case BossState::Attack3: // aimed shards
        if (b.sm.ticksIn() == 16 || b.sm.ticksIn() == 32 || (phase >= 1 && b.sm.ticksIn() == 48)) {
            bossFire(b, me, gamelib::core::normalized(hero - me) * 2.8f * fast, 4, 3);
        }
        if (b.sm.after(64)) {
            b.sm.enter(BossState::Idle);
        }
        break;
    case BossState::Recover:
        b.body.velocity.x = 0.0f;
        if (b.sm.after(kPhaseTicks)) {
            b.sm.enter(BossState::Idle);
        }
        break;
    default: b.sm.enter(BossState::Idle); break;
    }
    const gamelib::core::WalkerStep step =
        gamelib::core::stepWalker(b.body, {.gravity = kBossGravity, .maxFall = 6.0f}, world);
    if (b.sm.is(BossState::Attack) && b.sm.ticksIn() >= 22 && (step.contacts.blockedX || b.sm.after(160))) {
        b.body.velocity.x = 0.0f;
        b.body.direction = -b.body.direction;
        b.sm.enter(BossState::Recover);
    }
}

// --- THORNWOOD: lobs seeds, sends thorns up from the floor, hops.
void Session::stepThorn(Boss& b)
{
    const gamelib::core::TileWorld world = tileWorld();
    const Vec2 hero = m_hero.body.box.center();
    const Vec2 me = b.body.box.center();
    const int phase = b.phases.phase();
    if (b.body.grounded) {
        b.body.velocity.x = 0.0f;
    }
    switch (b.sm.current()) {
    case BossState::Idle:
        b.body.direction = _sign(hero.x - me.x);
        if (b.sm.after(46 - 8 * phase)) {
            b.pattern = gamelib::core::nextPattern(m_rng, 3, b.pattern);
            b.target = hero;
            b.sm.enter(b.pattern == 0 ? BossState::Attack : (b.pattern == 1 ? BossState::Attack2 : BossState::Jump));
        }
        break;
    case BossState::Attack: // a burst of seeds
        if (b.sm.ticksIn() == 20) {
            const int count = phase >= 1 ? 5 : 3;
            const Vec2 from = me + Vec2{0.0f, -14.0f};
            for (int i = 0; i < count; ++i) {
                const float offset = (static_cast<float>(i) - static_cast<float>(count - 1) / 2.0f) * 30.0f;
                bossFire(b, from, gamelib::ai::lobVelocity(from, {hero.x + offset, b.home.y - 4.0f}, 0.2f, 46, {3.2f, 7.0f}), 5, 3, 0.2f);
            }
        }
        if (b.sm.after(54)) {
            b.sm.enter(BossState::Idle);
        }
        break;
    case BossState::Attack2: // thorns under the hero
        if (b.sm.ticksIn() == 6) {
            warn({hero.x, b.home.y}, 46, 1);
            if (phase >= 1) {
                warn({hero.x - 56.0f, b.home.y}, 56, 1);
                warn({hero.x + 56.0f, b.home.y}, 56, 1);
            }
        }
        if (b.sm.after(74)) {
            b.sm.enter(BossState::Idle);
        }
        break;
    case BossState::Jump:
        if (b.sm.ticksIn() == 20) {
            b.body.velocity = {std::clamp((b.target.x - me.x) / 46.0f, -2.0f, 2.0f), -5.2f};
            b.body.grounded = false;
        }
        if (b.sm.ticksIn() > 22 && b.body.grounded) {
            m_events.push({.kind = EventKind::BossLand, .at = b.body.box.bottomCenter()});
            b.sm.enter(BossState::Idle);
        }
        break;
    case BossState::Recover:
        if (b.sm.after(kPhaseTicks)) {
            b.sm.enter(BossState::Idle);
        }
        break;
    default: b.sm.enter(BossState::Idle); break;
    }
    (void)gamelib::core::stepWalker(b.body, {.gravity = kBossGravity, .maxFall = 6.0f}, world);
}

// --- CINDERMAW: leaps that send fire along the floor, lobs bouncing magma.
void Session::stepCinder(Boss& b)
{
    const gamelib::core::TileWorld world = tileWorld();
    const Vec2 hero = m_hero.body.box.center();
    const Vec2 me = b.body.box.center();
    const int phase = b.phases.phase();
    if (b.body.grounded) {
        b.body.velocity.x = 0.0f;
    }
    const auto lob = [&] {
        const Vec2 from = me + Vec2{0.0f, -12.0f};
        Shot s;
        s.position = from;
        s.velocity = gamelib::ai::lobVelocity(from, {hero.x, b.home.y - 6.0f}, 0.2f, 44, {3.2f, 7.0f});
        s.gravity = 0.2f;
        s.size = {12.0f, 12.0f};
        s.team = 1;
        s.owner = 205;
        s.damage = 3;
        s.lifetimeTicks = 200;
        s.tileContact = gamelib::core::TileContact::BounceOnFloor;
        s.bounceSpeed = 3.0f;
        s.maxBounces = 2;
        s.extra = {.enemy = true, .look = 6};
        if (m_shots.spawn(s) != 0) {
            m_events.push({.kind = EventKind::EnemyShot, .at = from, .boss = b.kind});
        }
    };
    switch (b.sm.current()) {
    case BossState::Idle:
        b.body.direction = _sign(hero.x - me.x);
        if (b.sm.after(42 - 8 * phase)) {
            b.pattern = gamelib::core::nextPattern(m_rng, phase >= 2 ? 3 : 2, b.pattern);
            b.target = hero;
            b.sm.enter(b.pattern == 1 ? BossState::Attack : BossState::Jump);
            b.count = b.pattern == 2 ? 1 : 0; // the leap that lobs too
        }
        break;
    case BossState::Jump:
        if (b.sm.ticksIn() == 18) {
            b.body.velocity = {std::clamp((b.target.x - me.x) / 48.0f, -2.4f, 2.4f), -6.0f};
            b.body.grounded = false;
        }
        if (b.count > 0 && b.sm.ticksIn() == 34) {
            lob();
        }
        if (b.sm.ticksIn() > 20 && b.body.grounded) {
            m_events.push({.kind = EventKind::BossLand, .at = b.body.box.bottomCenter()});
            for (const float dir : {-1.0f, 1.0f}) {
                bossFire(b, {me.x + dir * 16.0f, b.home.y - 8.0f}, {dir * 2.0f, 0.0f}, 6, 3);
            }
            b.sm.enter(BossState::Idle);
        }
        break;
    case BossState::Attack:
        if (b.sm.ticksIn() == 18 || b.sm.ticksIn() == 38 || (phase >= 1 && b.sm.ticksIn() == 58)) {
            lob();
        }
        if (b.sm.after(76)) {
            b.sm.enter(BossState::Idle);
        }
        break;
    case BossState::Recover:
        if (b.sm.after(kPhaseTicks)) {
            b.sm.enter(BossState::Idle);
        }
        break;
    default: b.sm.enter(BossState::Idle); break;
    }
    (void)gamelib::core::stepWalker(b.body, {.gravity = kBossGravity, .maxFall = 6.0f}, world);
}

// --- GALEWING: swoops at the hero, fans of feathers, and (from half health) gusts.
void Session::stepGale(Boss& b)
{
    const Room& r = room();
    const Vec2 hero = m_hero.body.box.center();
    const Vec2 me = b.body.box.center();
    const int phase = b.phases.phase();
    const float fast = 1.0f + 0.18f * static_cast<float>(phase);
    const float hoverTop = _hoverY(b) - b.body.box.height;
    const auto feathers = [&](const int count, const bool homing) {
        const float base = std::atan2(hero.y - me.y, hero.x - me.x);
        for (int i = 0; i < count; ++i) {
            const float a = base + (static_cast<float>(i) - static_cast<float>(count - 1) / 2.0f) * 0.28f;
            Shot s;
            s.position = me;
            s.velocity = gamelib::core::fromAngle(a) * 2.0f;
            s.size = {8.0f, 8.0f};
            s.team = 1;
            s.owner = 206;
            s.damage = 3;
            s.lifetimeTicks = 200;
            s.tileContact = gamelib::core::TileContact::Ignore;
            s.motion = homing ? gamelib::core::ProjectileMotion::Homing : gamelib::core::ProjectileMotion::Straight;
            s.homingTurn = 0.015f;
            s.extra = {.enemy = true, .look = 7};
            (void)m_shots.spawn(s);
        }
        m_events.push({.kind = EventKind::EnemyShot, .at = me, .boss = b.kind});
    };
    switch (b.sm.current()) {
    case BossState::Idle:
        b.body.box.y = gamelib::core::approach(b.body.box.y, hoverTop + 5.0f * std::sin(static_cast<float>(m_ticks) * 0.07f), 1.5f);
        b.body.direction = _sign(hero.x - me.x);
        if (b.sm.after(34 - 8 * phase)) {
            b.pattern = gamelib::core::nextPattern(m_rng, phase >= 1 ? 3 : 2, b.pattern);
            b.target = hero;
            b.sm.enter(b.pattern == 0 ? BossState::Attack : (b.pattern == 1 ? BossState::Attack2 : BossState::Attack3));
        }
        break;
    case BossState::Attack: // a swoop: aim, dive to the hero's height, sweep up the far side
        if (b.sm.ticksIn() == 24) {
            b.body.velocity = gamelib::core::normalized(b.target - me) * 3.0f * fast;
            b.body.direction = _sign(b.body.velocity.x);
        }
        if (b.sm.ticksIn() > 24) {
            b.body.box.x += b.body.velocity.x;
            b.body.box.y += b.body.velocity.y;
            if (b.body.velocity.y > 0.0f && me.y >= b.target.y - 6.0f) {
                b.body.velocity = {static_cast<float>(b.body.direction) * 2.6f * fast, -1.9f};
            }
            const bool atWall = me.x < r.bounds.left() + 2.5f * kTile || me.x > r.bounds.right() - 2.5f * kTile;
            if ((b.body.velocity.y < 0.0f && me.y <= _hoverY(b) - b.body.box.height / 2.0f) || (atWall && b.sm.after(40))
                || b.sm.after(160)) {
                b.body.velocity = {};
                b.sm.enter(BossState::Idle);
            }
        }
        break;
    case BossState::Attack2:
        if (b.sm.ticksIn() == 18) {
            feathers(5, phase >= 1);
        }
        if (b.sm.after(46)) {
            b.sm.enter(BossState::Idle);
        }
        break;
    case BossState::Attack3: // a gust pushes the hero away, feathers ride it
        m_wind = me.x < hero.x ? 0.62f : -0.62f;
        if (b.sm.ticksIn() % 36 == 20) {
            feathers(3, false);
        }
        if (b.sm.after(128)) {
            m_wind = 0.0f;
            b.sm.enter(BossState::Idle);
        }
        break;
    case BossState::Recover:
        b.body.box.y = gamelib::core::approach(b.body.box.y, hoverTop, 1.5f);
        if (b.sm.after(kPhaseTicks)) {
            b.sm.enter(BossState::Idle);
        }
        break;
    default: b.sm.enter(BossState::Idle); break;
    }
}

// --- THE OVERSEER: orbs, a sweeping laser (jump it or slide under it), debris and drones.
void Session::stepOverseer(Boss& b)
{
    const Room& r = room();
    const Vec2 hero = m_hero.body.box.center();
    const Vec2 me = b.body.box.center();
    const int phase = b.phases.phase();
    const float fast = 1.0f + 0.2f * static_cast<float>(phase);
    b.body.box.y = _hoverY(b) - b.body.box.height + 6.0f * std::sin(static_cast<float>(m_ticks) * 0.04f);
    b.body.direction = _sign(hero.x - me.x);
    switch (b.sm.current()) {
    case BossState::Idle:
        if (b.sm.after(44 - 8 * phase)) {
            b.pattern = gamelib::core::nextPattern(m_rng, 1 + std::min(phase, 1) + (phase >= 2 ? 1 : 0), b.pattern);
            b.sm.enter(b.pattern == 0 ? BossState::Attack : (b.pattern == 1 ? BossState::Attack2 : BossState::Attack3));
            b.count = m_rng.range(0, 1); // the laser: 0 low (jump it), 1 high (slide under it)
        }
        break;
    case BossState::Attack: // a burst of orbs from the eye
        if (b.sm.ticksIn() == 16 || b.sm.ticksIn() == 30 || b.sm.ticksIn() == 44) {
            const Vec2 eye = me + Vec2{-10.0f, -6.0f};
            bossFire(b, eye, gamelib::core::normalized(hero - eye) * 2.4f * fast, 2, 4);
        }
        if (b.sm.after(64)) {
            b.sm.enter(BossState::Idle);
        }
        break;
    case BossState::Attack2: { // a laser across the room at one height
        const float y = b.count == 0 ? b.home.y - 6.0f : b.home.y - 21.0f;
        b.target = {r.bounds.center().x, y};
        if (b.sm.ticksIn() == 56) {
            Shot s;
            s.position = {r.bounds.center().x - kTile, y};
            s.size = {r.bounds.width - 2.0f * kTile, b.count == 0 ? 10.0f : 12.0f};
            s.team = 1;
            s.owner = 207;
            s.damage = 5;
            s.lifetimeTicks = 26;
            s.tileContact = gamelib::core::TileContact::Ignore;
            s.extra = {.enemy = true, .look = 8};
            (void)m_shots.spawn(s);
            m_events.push({.kind = EventKind::Strike, .at = {me.x, y}, .value = 4});
        }
        if (b.sm.after(96)) {
            b.sm.enter(BossState::Idle);
        }
        break;
    }
    case BossState::Attack3: // debris from the ceiling and two drones
        if (b.sm.ticksIn() == 6) {
            for (int i = 0; i < 4; ++i) {
                const float x = r.bounds.left() + 2.0f * kTile
                                + static_cast<float>(m_rng.range(0, static_cast<int>(r.bounds.width - 8.0f * kTile)));
                warn({x, b.home.y}, 44 + 8 * i, 3);
            }
        }
        if (b.sm.ticksIn() == 30) {
            for (const float dy : {-24.0f, 24.0f}) {
                Enemy d;
                d.kind = EnemyKind::Drone;
                d.hp = d.maxHp = 1;
                d.body.box = Rect::fromCenter(me + Vec2{-20.0f, dy}, {10.0f, 10.0f});
                d.previous = d.body.box;
                d.origin = me;
                d.body.velocity = {-1.0f, 0.0f};
                d.vulnerable = gamelib::core::Vulnerable{d.body.box, 1};
                m_enemies.insert(d);
            }
        }
        if (b.sm.after(80)) {
            b.sm.enter(BossState::Idle);
        }
        break;
    case BossState::Recover:
        if (b.sm.after(kPhaseTicks)) {
            b.sm.enter(BossState::Idle);
        }
        break;
    default: b.sm.enter(BossState::Idle); break;
    }
}

} // namespace blaster::game
