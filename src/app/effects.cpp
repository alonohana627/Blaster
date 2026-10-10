#include "effects.hpp"

#include "palette.hpp"

#include <gamelib/core/easing.hpp>

#include <cmath>
#include <numbers>

namespace blaster::app {

namespace {

// helpers

using gamelib::core::Color;
using gamelib::core::Vec2;
using game::EventKind;

constexpr float kPi = std::numbers::pi_v<float>;

Color _weaponGlow(const game::Weapon weapon)
{
    if (weapon == game::Weapon::Buster) {
        return srgb(0xFFF2A0);
    }
    return heroColors(weapon).trim;
}

} // namespace

Effects::Effects()
    : m_debris{400, {0.0f, 520.0f}, 0.5f}
    , m_dust{{.size = {1.6f, 2.8f}, .life = {0.25f, 0.4f}, .rise = {4.0f, 10.0f}, .jitter = 4.0f, .fanOut = 2.0f}}
    , m_sparks{{.start = 2.0f, .reach = 12.0f, .length = 3.0f, .thickness = 1.0f, .jitter = 0.3f}}
    , m_rng{gamelib::core::randomSeed()}
{
}

void Effects::clear()
{
    m_debris.clear();
    m_dust.clear();
    m_rings.clear();
    m_sparks.clear();
    m_flashes.clear();
    m_orbs.clear();
    m_flash.clear();
    m_hurt.clear();
    m_shake.clear();
}

void Effects::update(const float seconds)
{
    m_debris.update(seconds);
    m_dust.update(seconds);
    m_rings.update(seconds);
    m_sparks.update(seconds);
    m_flashes.update(seconds);
    m_orbs.update(seconds);
    m_flash.update(seconds);
    m_hurt.update(seconds);
    m_shake.update(seconds);
}

void Effects::burst(const Vec2 at, const Color color, const float size)
{
    m_rings.add(at, size, color, 0.5f);
    m_orbs.add({.at = at, .color = color, .speed = size * 2.5f}, 1.0f);
    m_sparks.burst(at, 8, color, 0.5f);
}

void Effects::onEvent(const game::Event& e, const game::Theme theme)
{
    const ThemePalette& pal = themePalette(theme);
    switch (e.kind) {
    case EventKind::Shoot:
        m_flashes.add({.at = e.at, .color = _weaponGlow(e.weapon), .size = e.value == 2 ? 13.0f : (e.value == 1 ? 9.0f : 6.0f)},
                      0.08f);
        break;
    case EventKind::ChargeReady: m_rings.add(e.at, 16.0f, srgb(0xFFF2A0), 0.3f); break;
    case EventKind::Land: m_dust.burst(e.at, 3, 3.0f, {}, pal[Role::GroundEdge].withAlpha(0.7f)); break;
    case EventKind::Slide:
        m_dust.burst(e.at + Vec2{static_cast<float>(-e.value) * 4.0f, 0.0f}, 3, 2.5f,
                     {static_cast<float>(-e.value) * 18.0f, 0.0f}, pal[Role::GroundEdge].withAlpha(0.7f));
        break;
    case EventKind::HitEnemy: m_sparks.burst(e.at, 4, srgb(0xFFFFFF), 0.2f); break;
    case EventKind::Deflect: m_sparks.burst(e.at, 3, srgb(0xC8D0E0), 0.2f); break;
    case EventKind::EnemyDown: {
        const bool big = e.value == 1;
        m_rings.add(e.at, big ? 40.0f : 16.0f, srgb(0xFFD27A), big ? 0.6f : 0.35f);
        m_flashes.add({.at = e.at, .color = srgb(0xFFF2C0), .size = big ? 26.0f : 12.0f}, 0.12f);
        m_sparks.burst(e.at, big ? 12 : 6, srgb(0xFFB040), 0.35f);
        m_debris.burst(e.at, big ? 18 : 7,
                       {.direction = {-kPi, 0.0f}, .speed = {50.0f, 140.0f}, .size = {1.5f, 3.0f}, .life = {0.4f, 0.8f},
                        .spin = {-8.0f, 8.0f}, .color = srgb(0x8A8E9C)});
        if (big) {
            m_orbs.add({.at = e.at, .color = srgb(0xFFB040), .speed = 90.0f}, 1.0f);
            m_shake.trigger(0.8f);
        }
        break;
    }
    case EventKind::Hurt:
        m_hurt.trigger();
        m_shake.trigger(0.35f);
        m_sparks.burst(e.at, 5, srgb(0xFFFFFF), 0.25f);
        break;
    case EventKind::Death:
        m_orbs.add({.at = e.at, .color = srgb(0x6FE0FF), .speed = 55.0f}, 1.6f);
        m_orbs.add({.at = e.at, .color = srgb(0x2F7BFF), .speed = 110.0f}, 1.6f);
        m_flash.trigger(0.6f);
        m_shake.trigger(0.7f);
        break;
    case EventKind::Pickup: m_rings.add(e.at, 10.0f, srgb(0xFFF2A0), 0.25f); break;
    case EventKind::Checkpoint: m_rings.add(e.at + Vec2{0.0f, -20.0f}, 22.0f, pal[Role::Accent], 0.5f); break;
    case EventKind::DoorOpen:
    case EventKind::DoorClose: m_shake.trigger(0.25f); break;
    case EventKind::Unlock: m_shake.trigger(0.4f); break;
    case EventKind::BossHit:
        m_sparks.burst(e.at, 7, srgb(0xFFFFFF), 0.3f);
        m_flashes.add({.at = e.at, .color = srgb(0xFFFFFF), .size = 12.0f}, 0.1f);
        break;
    case EventKind::BossPhase:
        m_rings.add(e.at, 40.0f, bossColors(e.boss).trim, 0.6f);
        m_flash.trigger(0.35f);
        break;
    case EventKind::BossLand:
        m_shake.trigger(0.6f);
        m_dust.burst(e.at, 6, 5.0f, {}, pal[Role::GroundEdge].withAlpha(0.8f));
        break;
    case EventKind::BossDown:
        for (int i = 0; i < 3; ++i) {
            m_orbs.add({.at = e.at, .color = i == 1 ? srgb(0xFFFFFF) : bossColors(e.boss).trim,
                        .speed = 45.0f + 40.0f * static_cast<float>(i)},
                       2.0f);
        }
        m_rings.add(e.at, 60.0f, srgb(0xFFFFFF), 0.8f);
        m_flash.trigger(1.0f);
        m_shake.trigger(1.0f);
        break;
    case EventKind::Strike:
        m_flashes.add({.at = e.at, .color = srgb(0xFFFFE0), .size = 18.0f}, 0.12f);
        m_shake.trigger(0.3f);
        if (e.value == 4) {
            m_flash.trigger(0.25f);
        }
        break;
    case EventKind::EnemyShot: m_flashes.add({.at = e.at, .color = kEnemyShot, .size = 6.0f}, 0.07f); break;
    case EventKind::Bomb:
        m_rings.add(e.at, 18.0f, srgb(0xFF8A3A), 0.35f);
        m_debris.burst(e.at, 6, {.direction = {-kPi, 0.0f}, .speed = {40.0f, 120.0f}, .size = {1.5f, 2.5f}, .life = {0.3f, 0.6f},
                                 .color = srgb(0xFFB040)});
        m_shake.trigger(0.25f);
        break;
    case EventKind::Tank: m_rings.add(e.at, 26.0f, srgb(0x7CFF8A), 0.5f); break;
    case EventKind::BeamIn:
    case EventKind::BeamOut: m_rings.add(e.at + Vec2{0.0f, -11.0f}, 18.0f, srgb(0x6FE0FF), 0.35f); break;
    case EventKind::WeaponSwitch: m_rings.add(e.at, 14.0f, heroColors(e.weapon).trim, 0.3f); break;
    default: break;
    }
}

void Effects::drawWorld(gamelib::draw::DrawList& list) const
{
    m_dust.draw(list);
    m_debris.draw(list);
    for (const auto& item : m_orbs) {
        const float t = item.progress();
        const float r = item.value.speed * t;
        const float fade = 1.0f - gamelib::core::easeInQuad(t);
        for (int i = 0; i < 8; ++i) {
            const float a = static_cast<float>(i) * kPi / 4.0f;
            const Vec2 p = item.value.at + Vec2{std::cos(a), std::sin(a)} * r;
            list.circle(p, 3.2f, item.value.color.withAlpha(fade),
                        {.glowWidth = 3.0f, .glowColor = item.value.color.withAlpha(0.5f * fade)});
        }
    }
    for (const auto& item : m_flashes) {
        const float t = item.progress();
        list.circle(item.value.at, item.value.size * (0.6f + 0.6f * t), item.value.color.withAlpha(1.0f - t),
                    {.glowWidth = item.value.size * 0.6f, .glowColor = item.value.color.withAlpha(0.5f * (1.0f - t))});
    }
    m_rings.draw(list);
    m_sparks.draw(list);
}

void Effects::drawFlash(gamelib::draw::DrawList& list, const gamelib::core::Size canvas) const
{
    gamelib::fx::drawFlash(list, canvas, m_flash, {1.0f, 1.0f, 1.0f, 0.55f});
}

} // namespace blaster::app
