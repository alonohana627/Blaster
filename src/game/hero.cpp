// The hero: GameLib's platformer controller plus the slide (a short box
// pushed along the floor that stands up only where there is room), the
// buster with its charge, the special weapons, shooting while hanging on a
// ladder, conveyors, ice and wind.
#include "session.hpp"

#include <gamelib/core/collision.hpp>

#include <algorithm>
#include <cmath>

namespace blaster::game {

namespace {

// helpers

using gamelib::core::Cell;
using gamelib::core::Rect;
using gamelib::core::TileShape;
using gamelib::core::Vec2;

constexpr float kConveyorSpeed = 0.7f;

// The tile under the middle of the feet.
gamelib::core::TileId _floorTile(const gamelib::core::TileMap& map, const Rect& box)
{
    const Cell cell = gamelib::core::cellAt(box.bottomCenter() + Vec2{0.0f, 1.0f}, kTile);
    const gamelib::core::TileLayer& layer = map.layer(0);
    return layer.inside(cell) ? layer.at(cell) : gamelib::core::TileId{' '};
}

} // namespace

bool Session::canStand() const
{
    const Rect full = Rect::fromBottomCenter(m_hero.body.box.bottomCenter(), kHeroSize);
    const gamelib::core::TileWorld world = tileWorld();
    const gamelib::core::CellRange cells = gamelib::core::cellsOverlapping(full, kTile);
    for (int y = cells.min.y; y <= cells.max.y; ++y) {
        for (int x = cells.min.x; x <= cells.max.x; ++x) {
            if (world.shapeAt({x, y}) == TileShape::Solid) {
                return false;
            }
        }
    }
    return true;
}

void Session::endSlide()
{
    Hero& h = m_hero;
    if (!h.sliding) {
        return;
    }
    h.sliding = false;
    h.slideTicks = 0;
    h.body.box = Rect::fromBottomCenter(h.body.box.bottomCenter(), kHeroSize);
}

void Session::switchWeapon(const int step)
{
    int w = static_cast<int>(m_hero.weapon);
    for (int i = 0; i < kWeaponCount; ++i) {
        w = (w + step + kWeaponCount) % kWeaponCount;
        if (owns(static_cast<Weapon>(w))) {
            selectWeapon(static_cast<Weapon>(w));
            return;
        }
    }
}

void Session::stepHero(const Input& input)
{
    Hero& h = m_hero;
    gamelib::core::PlatformerBody& body = h.body;
    const gamelib::core::TileWorld world = tileWorld();
    const bool stunned = h.vulnerable.stunned();
    const Input in = stunned ? Input{.shootHeld = input.shootHeld} : input;
    h.hurtTicks = std::max(0, h.hurtTicks - 1);
    h.shootTicks = std::max(0, h.shootTicks - 1);

    if (in.weaponPrev) {
        switchWeapon(-1);
    } else if (in.weaponNext) {
        switchWeapon(1);
    }

    std::vector<gamelib::core::MovingPlatform> platforms;
    for (const Platform& p : m_platforms) {
        if (p.room == m_room) {
            platforms.push_back({.box = p.box, .delta = p.delta, .id = p.id});
        }
    }

    // Down + jump (or the slide button) on the ground starts a slide.
    const bool slideAsked = in.slidePressed || (in.climb > 0 && in.jumpPressed);
    const bool startSlide = slideAsked && body.grounded && !body.climbing && !h.sliding;
    if (startSlide) {
        h.sliding = true;
        h.slideTicks = kSlideTicks;
        if (in.move != 0) {
            body.facing = in.move;
        }
        body.box = Rect::fromBottomCenter(body.box.bottomCenter(), kSlideSize);
        m_events.push({.kind = EventKind::Slide, .at = body.box.bottomCenter(), .value = body.facing});
    }

    const float xBefore = body.box.x;
    gamelib::core::PlatformerStep step;
    if (h.sliding) {
        if (in.move != 0 && in.move != body.facing && !startSlide) {
            body.facing = in.move; // turning round mid-slide
        }
        const bool room = canStand();
        const gamelib::core::PlatformerIntent intent{.move = static_cast<float>(body.facing),
                                                     .jumpPressed = in.jumpPressed && !startSlide && room,
                                                     .jumpHeld = in.jumpHeld};
        if (intent.jumpPressed) {
            endSlide();
            step = gamelib::core::stepPlatformer(body, intent, heroTuning(false), world, platforms);
        } else {
            step = gamelib::core::stepPlatformer(body, intent, slideTuning(), world, platforms);
            --h.slideTicks;
            if (!body.grounded || ((h.slideTicks <= 0 || step.contacts.blockedX) && canStand())) {
                endSlide();
            } else if (step.contacts.blockedX) {
                body.facing = -body.facing; // stuck under a ceiling against a wall: slide back out
            }
        }
    } else {
        gamelib::core::PlatformerIntent intent{.move = static_cast<float>(in.move),
                                               .climb = static_cast<float>(in.climb),
                                               .jumpPressed = in.jumpPressed && !slideAsked,
                                               .jumpHeld = in.jumpHeld};
        if (body.climbing) {
            if (in.move != 0) {
                body.facing = in.move; // turn on the ladder to shoot the other way
            }
            if (h.shootTicks > 0) {
                intent.climb = 0.0f; // hanging still while shooting
            }
        }
        const bool icy = body.grounded && _floorTile(m_map, body.box) == 'I';
        step = gamelib::core::stepPlatformer(body, intent, heroTuning(icy), world, platforms);
    }
    if (step.events.jumped) {
        m_events.push({.kind = EventKind::Jump, .at = body.box.bottomCenter()});
    }
    if (step.events.landed) {
        m_events.push({.kind = EventKind::Land, .at = body.box.bottomCenter()});
    }

    // Conveyors carry what stands on them; a gust pushes.
    Vec2 push;
    if (body.grounded) {
        const gamelib::core::TileId floor = _floorTile(m_map, body.box);
        if (floor == '<') {
            push.x -= kConveyorSpeed;
        } else if (floor == '>') {
            push.x += kConveyorSpeed;
        }
    }
    if (!body.climbing) {
        push.x += m_wind;
    }
    if (push.x != 0.0f) {
        const gamelib::core::BoxMove moved = gamelib::core::moveBox(body.box, push, world);
        body.box = moved.box;
    }
    if (body.grounded) {
        h.walked += std::abs(body.box.x - xBefore);
    }

    // Spikes spare a hero still flickering from a hit; lava and drops never do.
    if (step.contacts.hazard) {
        const gamelib::core::TileId id = m_map.layer(0).at(step.contacts.hazardCell);
        if (id == '~' || !h.vulnerable.invincible()) {
            die();
            return;
        }
    }
    h.vulnerable.setHurtbox(body.box.expanded(-2.0f));
    fire(in);
}

void Session::fire(const Input& in)
{
    Hero& h = m_hero;
    const gamelib::core::PlatformerBody& body = h.body;
    const auto dir = static_cast<float>(body.facing);
    const Vec2 hand = body.box.center() + Vec2{dir * 12.0f, body.climbing ? -3.0f : -1.0f};
    const auto shot = [&](const Vec2 velocity, const Vec2 size) {
        Shot s;
        s.position = hand;
        s.velocity = velocity;
        s.size = size;
        s.team = 0;
        s.owner = 1 + static_cast<int>(h.weapon);
        s.lifetimeTicks = 120;
        s.tileContact = gamelib::core::TileContact::Ignore;
        s.extra.weapon = h.weapon;
        return s;
    };

    if (h.weapon == Weapon::Buster) {
        // Charging: held fire builds up; the release fires what it reached.
        if (in.shootHeld && !h.sliding) {
            ++h.charge;
            if (h.charge == kChargeFullTicks) {
                m_events.push({.kind = EventKind::ChargeReady, .at = body.box.center()});
            }
        } else if (!in.shootHeld && h.charge > 0) {
            const int level = h.charge >= kChargeFullTicks ? 2 : (h.charge >= kChargeHalfTicks ? 1 : 0);
            h.charge = 0;
            if (level > 0 && !h.sliding) {
                Shot s = shot({dir * kBusterSpeed, 0.0f}, level == 2 ? Vec2{20.0f, 14.0f} : Vec2{11.0f, 9.0f});
                s.extra.charge = level;
                s.pierce = level == 2;
                if (m_shots.spawn(s) != 0) {
                    h.shootTicks = kShootPoseTicks;
                    m_events.push({.kind = EventKind::Shoot, .at = hand, .value = level, .weapon = Weapon::Buster});
                }
            }
        }
        if (in.shootPressed && !h.sliding) {
            h.charge = 1;
            if (m_shots.spawn(shot({dir * kBusterSpeed, 0.0f}, {7.0f, 6.0f}), weaponInfo(Weapon::Buster).maxShots) != 0) {
                h.shootTicks = kShootPoseTicks;
                m_events.push({.kind = EventKind::Shoot, .at = hand, .value = 0, .weapon = Weapon::Buster});
            }
        }
        return;
    }

    h.charge = 0;
    if (!in.shootPressed || h.sliding) {
        return;
    }
    const WeaponInfo& info = weaponInfo(h.weapon);
    gamelib::core::Resource& energy = h.energy[static_cast<std::size_t>(h.weapon)];
    if (!energy.canSpend(info.cost) || m_shots.countOf(1 + static_cast<int>(h.weapon)) >= info.maxShots) {
        return;
    }
    bool fired = false;
    switch (h.weapon) {
    case Weapon::Saw: {
        Shot s = shot({dir * 4.2f, 0.0f}, {14.0f, 14.0f});
        s.pierce = true;
        fired = m_shots.spawn(s) != 0;
        break;
    }
    case Weapon::Spark: {
        Shot s = shot({dir * 4.4f, 0.0f}, {12.0f, 10.0f});
        s.motion = gamelib::core::ProjectileMotion::Sine;
        s.sineAmplitude = 12.0f;
        s.sinePeriodTicks = 22;
        fired = m_shots.spawn(s) != 0;
        break;
    }
    case Weapon::Frost: {
        Shot s = shot({dir * 6.0f, 0.0f}, {18.0f, 7.0f});
        fired = m_shots.spawn(s) != 0;
        break;
    }
    case Weapon::Leaf: {
        for (const float vy : {-1.7f, 0.0f, 1.7f}) {
            Shot s = shot({dir * (vy == 0.0f ? 4.4f : 4.0f), vy}, {9.0f, 9.0f});
            fired = m_shots.spawn(s) != 0 || fired;
        }
        break;
    }
    case Weapon::Magma: {
        Shot s = shot({dir * 2.6f, -4.2f}, {12.0f, 12.0f});
        s.gravity = 0.25f;
        s.tileContact = gamelib::core::TileContact::BounceOnFloor;
        s.bounceSpeed = 3.6f;
        s.maxBounces = 3;
        s.lifetimeTicks = 180;
        fired = m_shots.spawn(s) != 0;
        break;
    }
    case Weapon::Gale: {
        Shot s = shot({dir * 3.4f, -0.6f}, {14.0f, 12.0f});
        s.motion = gamelib::core::ProjectileMotion::Homing;
        s.homingTurn = 0.09f;
        s.lifetimeTicks = 150;
        fired = m_shots.spawn(s) != 0;
        break;
    }
    case Weapon::Buster:
    case Weapon::Count: break;
    }
    if (fired) {
        (void)energy.spend(info.cost);
        h.shootTicks = kShootPoseTicks;
        m_events.push({.kind = EventKind::Shoot, .at = hand, .value = 0, .weapon = h.weapon});
    }
}

} // namespace blaster::game
