#include "view.hpp"

#include "actions.hpp"
#include "palette.hpp"

#include "backdrop_frag.spv.hpp"
#include "boss_frag.spv.hpp"
#include "enemy_frag.spv.hpp"
#include "hero_frag.spv.hpp"
#include "item_frag.spv.hpp"
#include "shot_frag.spv.hpp"
#include "tile_frag.spv.hpp"

#include <gamelib/draw/parallax.hpp>
#include <gamelib/draw/tile_map.hpp>
#include <gamelib/draw/vector_text.hpp>
#include <gamelib/fx/sprite_motion.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace blaster::app {

namespace {

// helpers

using gamelib::core::Color;
using gamelib::core::Rect;
using gamelib::core::Vec2;
using game::kTile;

constexpr float kPi = std::numbers::pi_v<float>;

enum TileKind : int {
    kGround = 0,
    kBlock = 1,
    kOneWay = 2,
    kLadder = 3,
    kSpikes = 4,
    kLava = 5,
    kIce = 6,
    kConveyorLeft = 7,
    kConveyorRight = 8,
    kShutter = 9,
    kRailAcross = 10,
    kRailUp = 11,
    kDoor = 12,
};

gamelib::draw::TileSet _tileSet(const Shapes& shapes, const game::Theme theme)
{
    const ThemePalette& pal = themePalette(theme);
    const float t = themeIndex(theme);
    gamelib::draw::TileSet set;
    const auto style = [&](const char id, const int kind, const Color c, const Color c2, const std::uint8_t group) {
        set.set(gamelib::core::charTile(id),
                {.shape = shapes.tile, .color = c, .color2 = c2, .params = {static_cast<float>(kind), t, 0.0f, 0.0f}, .group = group});
    };
    const bool fiery = theme == game::Theme::Magma;
    style('#', kGround, pal[Role::Ground], pal[Role::GroundEdge], 0);
    style('B', kBlock, pal[Role::Block], pal[Role::BlockEdge], 1);
    style('=', kOneWay, pal[Role::Platform], pal[Role::GroundEdge], 2);
    style('H', kLadder, pal[Role::Ladder].scaled(0.8f), pal[Role::Ladder], 3);
    style('^', kSpikes, srgb(0x9AA0B4), srgb(0xF0F4FF), 4);
    style('~', kLava, fiery || theme == game::Theme::Lab ? srgb(0xC8300A) : srgb(0x2A8A3A),
          fiery || theme == game::Theme::Lab ? srgb(0xFFB020) : srgb(0x9AFF5A), 5);
    style('I', kIce, srgb(0x9ED8F8), srgb(0xE8F8FF), 6);
    style('<', kConveyorLeft, pal[Role::Block], pal[Role::Accent], 7);
    style('>', kConveyorRight, pal[Role::Block], pal[Role::Accent], 7);
    style('L', kShutter, srgb(0xE8C020), srgb(0x202020), 8);
    style('-', kRailAcross, pal[Role::Platform].scaled(0.6f), pal[Role::Platform], 9);
    style('|', kRailUp, pal[Role::Platform].scaled(0.6f), pal[Role::Platform], 9);
    return set;
}

struct EnemyLook {
    Vec2 size;
    Color body;
    Color accent;
};

EnemyLook _enemyLook(const game::EnemyKind kind)
{
    using game::EnemyKind;
    switch (kind) {
    case EnemyKind::Crawler: return {{18.0f, 16.0f}, srgb(0xF2C230), srgb(0x5AE0FF)};
    case EnemyKind::Hopper: return {{18.0f, 22.0f}, srgb(0x5AC85A), srgb(0xFF4A5A)};
    case EnemyKind::Diver: return {{22.0f, 16.0f}, srgb(0xC85AE0), srgb(0xFFE14D)};
    case EnemyKind::ShieldWalker: return {{24.0f, 24.0f}, srgb(0x4A8AE0), srgb(0xE8E8F0)};
    case EnemyKind::Turret: return {{22.0f, 20.0f}, srgb(0x6A6E7E), srgb(0xFF6A3A)};
    case EnemyKind::Nest: return {{26.0f, 26.0f}, srgb(0x8A5A3A), srgb(0xFFB040)};
    case EnemyKind::Drone: return {{14.0f, 16.0f}, srgb(0xE84A5A), srgb(0xFFE14D)};
    case EnemyKind::Roller: return {{20.0f, 20.0f}, srgb(0x5A5E70), srgb(0xE8ECF8)};
    case EnemyKind::Lobber: return {{20.0f, 22.0f}, srgb(0xE07A2A), srgb(0x6AE0FF)};
    case EnemyKind::Popper: return {{20.0f, 18.0f}, srgb(0x3AB0A0), srgb(0xF2C230)};
    case EnemyKind::Wisp: return {{26.0f, 16.0f}, srgb(0xB8F0FF), srgb(0x5AA8FF)};
    case EnemyKind::Dropper: return {{24.0f, 16.0f}, srgb(0x6A4AA8), srgb(0xFF5A8A)};
    case EnemyKind::MiniBoss: return {{46.0f, 40.0f}, srgb(0x5A6A8A), srgb(0xFFB040)};
    case EnemyKind::Count: break;
    }
    return {{16.0f, 16.0f}, srgb(0xFFFFFF), srgb(0xFFFFFF)};
}

// Each shot's look number in shot.frag and its colors.
struct ShotLook {
    float look = 0.0f;
    Color main;
    Color glow;
    float margin = 4.0f;
    Vec2 size;
};

ShotLook _shotLook(const game::Shot& s)
{
    if (!s.extra.enemy) {
        const game::Weapon w = s.extra.weapon;
        if (w == game::Weapon::Buster) {
            const int c = s.extra.charge;
            return {static_cast<float>(c), c == 2 ? srgb(0xF4FFF8) : srgb(0xFFF6C0), c == 2 ? srgb(0x6FE0FF) : srgb(0xFFC23A),
                    c == 2 ? 8.0f : 4.0f, c == 2 ? Vec2{36.0f, 16.0f} : (c == 1 ? Vec2{12.0f, 10.0f} : Vec2{8.0f, 6.0f})};
        }
        const HeroColors hc = heroColors(w);
        switch (w) {
        case game::Weapon::Saw: return {3.0f, hc.main.scaled(1.3f), hc.trim, 3.0f, {16.0f, 16.0f}};
        case game::Weapon::Spark: return {4.0f, srgb(0xFFFAD0), srgb(0xFFE14D), 5.0f, {12.0f, 12.0f}};
        case game::Weapon::Frost: return {5.0f, srgb(0xE8FCFF), srgb(0x6FD0FF), 4.0f, {20.0f, 8.0f}};
        case game::Weapon::Leaf: return {6.0f, srgb(0x6AD85A), srgb(0xE8F0A0), 2.0f, {12.0f, 12.0f}};
        case game::Weapon::Magma: return {7.0f, srgb(0xFFB030), srgb(0xFF4A1A), 5.0f, {14.0f, 14.0f}};
        case game::Weapon::Gale: return {8.0f, srgb(0xF8FCFF), srgb(0x6AB8FF), 4.0f, {14.0f, 12.0f}};
        default: break;
        }
        return {0.0f, srgb(0xFFF6C0), srgb(0xFFC23A), 4.0f, {8.0f, 6.0f}};
    }
    switch (s.extra.look) {
    case 0: return {10.0f, srgb(0xFFD27A), srgb(0xFF7A2A), 3.0f, {8.0f, 8.0f}};
    case 1: return {11.0f, srgb(0x303038), srgb(0xFFB040), 4.0f, {12.0f, 14.0f}};
    case 2: return {12.0f, srgb(0xFF8AB0), srgb(0xFF3A6A), 4.0f, {11.0f, 11.0f}};
    case 3: return {13.0f, srgb(0xFFFAD0), srgb(0xFFE14D), 5.0f, {10.0f, 10.0f}};
    case 4: return {14.0f, srgb(0xE8FCFF), srgb(0x6FD0FF), 3.0f, {s.size.y + 2.0f, 8.0f}};
    case 5: return {15.0f, srgb(0x8AD84A), srgb(0x3A7A2A), 2.0f, {10.0f, 10.0f}};
    case 6: return {16.0f, srgb(0xFFB030), srgb(0xFF4A1A), 5.0f, {s.size.x + 2.0f, s.size.y + 2.0f}};
    case 7: return {17.0f, srgb(0xF8FCFF), srgb(0x9AD0FF), 3.0f, {12.0f, 6.0f}};
    case 8: return {18.0f, srgb(0xFF8A9A), srgb(0xFF2A3A), 8.0f, s.size};
    case 9: return {19.0f, srgb(0xFFFAE0), srgb(0xC8B0FF), 10.0f, s.size};
    case 10: return {20.0f, srgb(0x4A9A3A), srgb(0xC8F07A), 2.0f, s.size};
    case 11: return {21.0f, srgb(0x6A6470), srgb(0xA89AB0), 2.0f, {10.0f, 10.0f}};
    case 12: return {22.0f, srgb(0xFF9A3A), srgb(0xFF5A1A), 6.0f, s.size};
    case 13: return {23.0f, srgb(0x9A9EAC), srgb(0xFF8A2A), 3.0f, {16.0f, 16.0f}};
    default: break;
    }
    return {10.0f, srgb(0xFFD27A), srgb(0xFF7A2A), 3.0f, {8.0f, 8.0f}};
}

} // namespace

Shapes Shapes::registerWith(gamelib::gfx::Renderer& renderer)
{
    return {.tile = renderer.addShape(shaders::tile_frag),
            .hero = renderer.addShape(shaders::hero_frag),
            .enemy = renderer.addShape(shaders::enemy_frag),
            .boss = renderer.addShape(shaders::boss_frag),
            .shot = renderer.addShape(shaders::shot_frag),
            .item = renderer.addShape(shaders::item_frag),
            .backdrop = renderer.addShape(shaders::backdrop_frag)};
}

StageView::StageView(const gamelib::core::Size canvas)
    : m_canvas{canvas}
    , m_camera{canvas, kZoom}
{
}

void StageView::snap(const game::Session& session)
{
    m_room = session.roomIndex();
    m_camera.setBounds(session.room().bounds);
    m_camera.centerOn(session.hero().body.box.center());
}

void StageView::follow(const game::Session& session, const float alpha, const float seconds)
{
    const int target = session.scrollTarget();
    if (target != m_room) {
        m_room = target;
        m_camera.setBounds(session.rooms()[static_cast<std::size_t>(target)].bounds,
                           static_cast<float>(game::kScrollTicks) / static_cast<float>(game::kTickRate));
    }
    const game::Hero& hero = session.hero();
    const Rect box = gamelib::core::lerp(hero.previous, hero.body.box, alpha);
    m_camera.follow({.position = box.center(), .facing = 0.0f, .grounded = true}, seconds, {.rate = 0.0f});
}

void StageView::draw(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session, const Effects& effects,
                     const StageDrawOptions& options) const
{
    list.setView({});
    drawBackdrop(list, shapes, session);
    drawHints(list, session, options);
    list.setView(m_camera.view(options.shake));
    drawTiles(list, shapes, session);
    drawThings(list, shapes, session, options.alpha);
    drawEnemies(list, shapes, session, options.alpha);
    drawBosses(list, shapes, session, options.alpha);
    drawHeroInStage(list, shapes, session, options.alpha);
    drawShots(list, shapes, session);
    drawWarnings(list, session);
    list.setLayer(gamelib::draw::layers::Effects);
    effects.drawWorld(list);
    list.setView({});
    drawWeather(list, shapes, session);
}

void StageView::drawBackdrop(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session) const
{
    const game::Theme theme = session.def().theme;
    const ThemePalette& pal = themePalette(theme);
    const float t = themeIndex(theme);
    list.setLayer(gamelib::draw::layers::Background);
    const std::array layers{
        gamelib::draw::ParallaxLayer{.shape = shapes.backdrop, .factor = {0.05f, 0.02f}, .color = pal[Role::SkyTop],
                                     .color2 = pal[Role::SkyLow], .params = {t, 0.0f}},
        gamelib::draw::ParallaxLayer{.shape = shapes.backdrop, .factor = {0.15f, 0.05f}, .color = pal[Role::Far],
                                     .color2 = pal[Role::FarLight], .params = {t, 1.0f}},
        gamelib::draw::ParallaxLayer{.shape = shapes.backdrop, .factor = {0.35f, 0.08f}, .color = pal[Role::Near],
                                     .color2 = pal[Role::NearLight], .params = {t, 2.0f}},
    };
    gamelib::draw::drawParallax(list, layers, m_camera.view(), m_canvas);
}

void StageView::drawWeather(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session) const
{
    const game::Theme theme = session.def().theme;
    if (theme == game::Theme::Fortress) {
        return;
    }
    Color c = srgb(0xFFFFFF, 0.5f);
    switch (theme) {
    case game::Theme::Storm: c = srgb(0xB8C0FF, 0.55f); break;
    case game::Theme::Magma: c = srgb(0xFF8A2A, 0.9f); break;
    case game::Theme::Forest: c = srgb(0x9AD860, 0.7f); break;
    case game::Theme::Factory: c = srgb(0xD8C8B8, 0.25f); break;
    case game::Theme::Lab: c = srgb(0xA8E8FF, 0.35f); break;
    case game::Theme::Sky: c = srgb(0xFFF4C0, 0.7f); break;
    default: break;
    }
    list.setLayer(gamelib::draw::layers::TilesFront);
    const std::array layers{gamelib::draw::ParallaxLayer{.shape = shapes.backdrop, .factor = {1.2f, 1.1f}, .color = c,
                                                         .color2 = c, .params = {themeIndex(theme), 3.0f}}};
    gamelib::draw::drawParallax(list, layers, m_camera.view(), m_canvas);
}

void StageView::drawHints(gamelib::draw::DrawList& list, const game::Session& session, const StageDrawOptions& options) const
{
    if (options.actions == nullptr) {
        return;
    }
    const gamelib::core::View2D view = m_camera.view(options.shake);
    list.setLayer(gamelib::draw::layers::Parallax + 50);
    for (const game::Hint& hint : session.def().hints) {
        const game::Room& room = session.rooms()[static_cast<std::size_t>(hint.room)];
        const Vec2 at = room.bounds.position() + hint.cell * kTile;
        const std::string text = withKeys(hint.text, *options.actions);
        constexpr float kSize = 40.0f;
        if (!gamelib::draw::drawWorldText(list, view, m_canvas, at, text,
                                          {.size = kSize, .color = Color{0.0f, 0.0f, 0.0f, 0.5f}, .weight = 1.6f,
                                           .align = gamelib::draw::Align::Center},
                                          {2.0f, 3.0f})) {
            continue;
        }
        gamelib::draw::drawWorldText(list, view, m_canvas, at, text,
                                     {.size = kSize, .color = srgb(0xFFF2C0, 0.95f), .weight = 1.2f,
                                      .align = gamelib::draw::Align::Center});
        // A soft plate behind the words (a layer below them), so they read over a busy backdrop.
        const Vec2 c = view.toCanvas(at);
        const float width = gamelib::draw::vectorTextWidth(text, kSize);
        list.setLayer(gamelib::draw::layers::Parallax + 40);
        list.box({c.x - width / 2.0f - 24.0f, c.y - 8.0f, width + 48.0f, kSize + 18.0f}, Color{0.02f, 0.03f, 0.08f, 0.75f},
                 {.cornerRadius = 14.0f, .borderWidth = 2.0f, .borderColor = srgb(0xFFF2C0, 0.35f)});
        list.setLayer(gamelib::draw::layers::Parallax + 50);
    }
}

void StageView::drawTiles(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session) const
{
    const game::Theme theme = session.def().theme;
    const gamelib::draw::TileSet tiles = _tileSet(shapes, theme);
    const gamelib::core::View2D view = m_camera.view();
    const Rect visible = view.visibleWorld(m_canvas).expanded(kTile);
    list.setLayer(gamelib::draw::layers::TilesBehind);
    gamelib::draw::drawTiles(list, session.map(), 0, tiles, visible);
    // Boss doors: slats that slide up as they open.
    const ThemePalette& pal = themePalette(theme);
    const std::span<const game::Door> doors = session.doors();
    for (std::size_t d = 0; d < doors.size(); ++d) {
        const game::Door& door = doors[d];
        const float top = static_cast<float>(door.cells.front().y) * kTile;
        const float height = static_cast<float>(door.cells.size()) * kTile;
        const float lower = top + height * (1.0f - session.doorOpen(static_cast<int>(d)));
        for (const gamelib::core::Cell c : door.cells) {
            const Rect r = gamelib::core::cellRect(c, kTile);
            if (!gamelib::core::overlaps(r, visible)) {
                continue;
            }
            list.add({.rect = r, .color = pal[Role::Block], .color2 = pal[Role::Accent], .shape = shapes.tile,
                      .params = {static_cast<float>(kDoor), themeIndex(theme), lower, 0.0f},
                      .bits = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(c.x)) << 32)
                              | (static_cast<std::uint64_t>(static_cast<std::uint32_t>(c.y)) << 48)});
        }
    }
}

void StageView::drawThings(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session,
                           const float alpha) const
{
    const ThemePalette& pal = themePalette(session.def().theme);
    // Checkpoint beacons.
    list.setLayer(gamelib::draw::layers::TilesBehind - 10);
    const std::span<const game::Checkpoint> checkpoints = session.checkpoints();
    for (std::size_t i = 0; i < checkpoints.size(); ++i) {
        const game::Checkpoint& c = checkpoints[i];
        const bool lit = static_cast<int>(i) <= session.checkpointReached();
        list.add({.rect = Rect::fromBottomCenter(c.feet, {12.0f, 34.0f}), .color = srgb(0x8A90A8), .color2 = pal[Role::Accent],
                  .shape = shapes.item, .params = {6.0f, 0.0f, lit ? 1.0f : 0.0f, 0.0f}});
    }
    // Lifts and falling platforms.
    list.setLayer(gamelib::draw::layers::Entities - 20);
    for (const game::Platform& p : session.platforms()) {
        if (p.room != session.roomIndex()) {
            continue;
        }
        const Rect box = gamelib::core::lerp(p.previous, p.box, alpha);
        const Color top = p.falling ? srgb(0xE0A050) : pal[Role::Platform];
        list.box(box, top, {.cornerRadius = 2.0f, .borderWidth = 1.0f, .borderColor = top.scaled(0.5f)});
        list.box({box.x + 3.0f, box.y + box.height, box.width - 6.0f, 2.0f}, top.scaled(0.4f));
    }
    // Pickups.
    list.setLayer(gamelib::draw::layers::Entities - 5);
    for (const game::Pickup& p : session.pickups()) {
        if (p.room != session.roomIndex()) {
            continue;
        }
        const Rect box = gamelib::core::lerp(p.previous, p.body.box, alpha);
        float kind = 0.0f;
        Color c1 = srgb(0xFF5A6A);
        Color c2 = srgb(0xFFE7A0);
        float size = 12.0f;
        switch (p.kind) {
        case game::PickupKind::SmallHealth: kind = 0.0f; size = 11.0f; break;
        case game::PickupKind::BigHealth: kind = 1.0f; size = 17.0f; break;
        case game::PickupKind::SmallEnergy: kind = 2.0f; c1 = srgb(0x5AB0FF); c2 = srgb(0xD8F4FF); size = 11.0f; break;
        case game::PickupKind::BigEnergy: kind = 3.0f; c1 = srgb(0x5AB0FF); c2 = srgb(0xD8F4FF); size = 17.0f; break;
        case game::PickupKind::ExtraLife: kind = 4.0f; c1 = heroColors(game::Weapon::Buster).main; c2 = heroColors(game::Weapon::Buster).trim; size = 15.0f; break;
        case game::PickupKind::Tank: kind = 5.0f; c1 = srgb(0x2F7BFF); c2 = srgb(0x6FE0FF); size = 16.0f; break;
        }
        const bool fading = p.life > 0 && p.life < 120 && (session.ticks() / 4) % 2 == 0;
        list.add({.rect = Rect::fromCenter(box.center(), {size + 4.0f, size + 4.0f}), .color = c1, .color2 = c2,
                  .shape = shapes.item, .params = {kind, fading ? 0.7f : 0.0f, 0.0f, 2.0f}});
    }
}

void StageView::drawEnemies(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session,
                            const float alpha) const
{
    const ThemePalette& pal = themePalette(session.def().theme);
    list.setLayer(gamelib::draw::layers::Entities);
    const Vec2 hero = session.hero().body.box.center();
    for (const auto [handle, e] : session.enemies()) {
        const Rect box = gamelib::core::lerp(e.previous, e.body.box, alpha);
        EnemyLook look = _enemyLook(e.kind);
        if (e.kind == game::EnemyKind::MiniBoss) {
            look.accent = pal[Role::Accent];
            look.body = gamelib::core::lerp(look.body, pal[Role::Block], 0.4f);
        }
        float phase = 0.0f;
        std::uint64_t bits = 0;
        switch (e.kind) {
        case game::EnemyKind::Crawler: phase = std::fmod(std::abs(box.x) / 15.0f, 1.0f); break;
        case game::EnemyKind::Diver: phase = std::fmod(static_cast<float>(e.age) / 14.0f, 1.0f); bits |= e.state == 1 ? 8u : 0u; break;
        case game::EnemyKind::ShieldWalker: phase = std::fmod(std::abs(box.x) / 12.0f, 1.0f); break;
        case game::EnemyKind::Turret: {
            const Vec2 d = hero - box.center();
            phase = std::atan2(d.y, std::abs(d.x));
            break;
        }
        case game::EnemyKind::Roller: phase = box.x / (2.0f * kPi * 7.0f); break;
        case game::EnemyKind::Hopper:
        case game::EnemyKind::Dropper: bits |= !e.body.grounded || e.state > 0 ? 8u : 0u; break;
        default: phase = std::fmod(static_cast<float>(e.age) / 60.0f, 1.0f); break;
        }
        if (!e.vulnerable.guarded()) {
            bits |= 1u;
        }
        if (e.flash > 0 && (e.flash / 2) % 2 == 0) {
            bits |= 2u;
        }
        if (e.frozen > 0) {
            bits |= 4u;
        }
        const float margin = 3.0f;
        list.add({.rect = Rect::fromCenter(box.center(), look.size + Vec2{2.0f * margin, 2.0f * margin}), .color = look.body,
                  .color2 = look.accent, .shape = shapes.enemy,
                  .params = {static_cast<float>(e.kind), static_cast<float>(e.body.direction), phase, margin}, .bits = bits});
    }
}

void StageView::drawBosses(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session,
                           const float alpha) const
{
    list.setLayer(gamelib::draw::layers::Entities + 2);
    const game::Boss* boss = session.activeBoss();
    if (boss == nullptr || boss->sm.is(game::BossState::Dead)) {
        return;
    }
    const Rect box = gamelib::core::lerp(boss->previous, boss->body.box, alpha);
    int pose = 0;
    switch (boss->sm.current()) {
    case game::BossState::Attack:
    case game::BossState::Attack2:
    case game::BossState::Attack3: pose = boss->sm.ticksIn() < 22 ? 1 : 3; break;
    case game::BossState::Jump:
    case game::BossState::Move: pose = 2; break;
    default: pose = 0; break;
    }
    if (boss->kind == game::BossKind::Grinder && boss->sm.is(game::BossState::Attack2) && boss->sm.ticksIn() >= 30) {
        pose = 2;
    }
    if (boss->kind == game::BossKind::Rime && boss->sm.is(game::BossState::Attack) && boss->sm.ticksIn() >= 22) {
        pose = 2;
    }
    std::uint64_t bits = 0;
    if (boss->phases.phase() >= 1) {
        bits |= 1u;
    }
    if (boss->phases.phase() >= 2) {
        bits |= 4u;
    }
    if (boss->flash > 0 && (boss->flash / 3) % 2 == 0) {
        bits |= 2u;
    }
    const HeroColors c = bossColors(boss->kind);
    const float margin = 8.0f;
    const Vec2 size = boss->kind == game::BossKind::Overseer ? Vec2{80.0f, 80.0f} : Vec2{50.0f, 50.0f};
    list.add({.rect = Rect::fromCenter(box.center(), size + Vec2{2.0f * margin, 2.0f * margin}), .color = c.main, .color2 = c.trim,
              .shape = shapes.boss,
              .params = {static_cast<float>(boss->kind), static_cast<float>(boss->body.direction), static_cast<float>(pose), margin},
              .bits = bits});
}

void StageView::drawShots(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session) const
{
    list.setLayer(gamelib::draw::layers::Entities + 10);
    for (const game::Shot& s : session.shots().items()) {
        const ShotLook look = _shotLook(s);
        float heading = std::atan2(s.velocity.y, s.velocity.x);
        if (s.extra.enemy && (s.extra.look == 4)) {
            heading = s.velocity.y > 0.5f ? kPi / 2.0f : heading;
        }
        if (look.look >= 18.0f && look.look <= 22.0f) {
            heading = 0.0f;
        }
        list.add({.rect = Rect::fromCenter(s.position, look.size + Vec2{2.0f * look.margin, 2.0f * look.margin}),
                  .color = look.main, .color2 = look.glow, .shape = shapes.shot,
                  .params = {look.look, heading, static_cast<float>(s.age), look.margin}});
    }
}

void StageView::drawWarnings(gamelib::draw::DrawList& list, const game::Session& session) const
{
    list.setLayer(gamelib::draw::layers::Effects - 5);
    const Rect room = session.room().bounds;
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(session.ticks()) * 0.6f);
    for (const auto& item : session.warnings()) {
        const game::Session::Warning& w = item.value;
        Color c = w.kind == 0 ? srgb(0xFFF080) : (w.kind == 1 ? srgb(0x9AFF6A) : srgb(0xC8F0FF));
        if (w.kind == 3) {
            c = srgb(0xFF8A6A);
        }
        const float a = 0.35f + 0.45f * pulse;
        if (w.kind == 0) {
            // A thin column from the sky where the bolt will fall.
            list.box({w.at.x - 1.0f, room.top() + 2.0f * kTile, 2.0f, w.at.y - room.top() - 2.0f * kTile}, c.withAlpha(0.25f * a));
        }
        if (w.kind == 2 || w.kind == 3) {
            // A shaking shape at the ceiling.
            const float shake = std::sin(static_cast<float>(session.ticks()) * 1.7f) * 1.0f;
            list.circle({w.at.x + shake, room.top() + 2.0f * kTile + 4.0f}, 3.0f, c.withAlpha(0.9f));
        }
        list.box({w.at.x - 9.0f, w.at.y - 2.0f, 18.0f, 2.0f}, c.withAlpha(a), {.cornerRadius = 1.0f, .glowWidth = 3.0f,
                                                                              .glowColor = c.withAlpha(0.5f * a)});
    }
}

void StageView::drawHero(gamelib::draw::DrawList& list, const Shapes& shapes, const Rect rect, const game::Weapon weapon,
                         const int facing, const int pose, const float phase, const bool shooting, const float charge,
                         const float scale)
{
    const HeroColors c = heroColors(weapon);
    std::uint64_t bits = shooting ? 1u : 0u;
    bits |= static_cast<std::uint64_t>(std::clamp(charge, 0.0f, 1.0f) * 255.0f) << 8;
    if (charge >= 1.0f) {
        bits |= 0x10000u;
    }
    list.add({.rect = rect, .color = c.main, .color2 = c.trim, .shape = shapes.hero,
              .params = {static_cast<float>(facing), static_cast<float>(pose), phase, 3.0f}, .bits = bits, .scale = scale});
}

void StageView::drawBoss(gamelib::draw::DrawList& list, const Shapes& shapes, const Vec2 center, const game::BossKind boss,
                         const int facing, const int pose, const float scale, const float alpha)
{
    const HeroColors c = bossColors(boss);
    const Vec2 size = boss == game::BossKind::Overseer ? Vec2{96.0f, 96.0f} : Vec2{66.0f, 66.0f};
    list.add({.rect = Rect::fromCenter(center, size * scale), .color = c.main.withAlpha(alpha), .color2 = c.trim, .shape = shapes.boss,
              .params = {static_cast<float>(boss), static_cast<float>(facing), static_cast<float>(pose), 8.0f}, .scale = scale});
}

void StageView::drawHeroInStage(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session,
                                const float alpha) const
{
    const game::Hero& hero = session.hero();
    const game::Phase phase = session.phase();
    if (phase == game::Phase::Dying || (phase == game::Phase::Over && session.outcome() == game::Outcome::Died)) {
        return;
    }
    const Rect box = gamelib::core::lerp(hero.previous, hero.body.box, alpha);
    const Vec2 feet = box.bottomCenter();
    const Vec2 drawSize{30.0f, 30.0f};
    list.setLayer(gamelib::draw::layers::Entities + 5);
    const auto draw = [&](const Rect& world, const int pose, const float ph, const bool shooting, const float charge) {
        drawHero(list, shapes, world, hero.weapon, hero.body.facing, pose, ph, shooting, charge, 1.0f);
    };
    // Beaming in at the start and out at the end: a falling or rising column.
    const int beamIn = game::kReadyTicks - 18;
    if (phase == game::Phase::Ready) {
        if (session.phaseTicks() < beamIn) {
            return;
        }
        const float t = static_cast<float>(session.phaseTicks() - beamIn) / 18.0f;
        const float top = session.room().bounds.top() - drawSize.y;
        const Vec2 at{feet.x, std::lerp(top, feet.y, std::min(t, 1.0f))};
        draw(Rect::fromBottomCenter(at, drawSize + Vec2{6.0f, 6.0f}), 6, 0.0f, false, 0.0f);
        return;
    }
    if (phase == game::Phase::BossDown || (phase == game::Phase::Over && session.outcome() == game::Outcome::Cleared)) {
        const int t = phase == game::Phase::Over ? 999 : session.phaseTicks() - 120;
        if (t >= 0) {
            const float rise = static_cast<float>(t) * 7.0f;
            if (rise < 300.0f) {
                draw(Rect::fromBottomCenter(feet - Vec2{0.0f, rise}, drawSize + Vec2{6.0f, 6.0f}), 6, 0.0f, false, 0.0f);
            }
            return;
        }
    }
    if (phase == game::Phase::Playing && !hero.vulnerable.visible(4) && hero.hurtTicks == 0) {
        return; // the flicker after a hit
    }
    int pose = 0;
    float ph = 0.0f;
    if (hero.hurtTicks > 0) {
        pose = 5;
    } else if (hero.body.sliding) {
        pose = 4;
    } else if (hero.body.climbing) {
        pose = 3;
        ph = std::fmod(box.y / 24.0f, 1.0f);
        if (ph < 0.0f) {
            ph += 1.0f;
        }
    } else if (!hero.body.grounded && phase != game::Phase::BossIntro) {
        pose = 2;
    } else if (std::abs(hero.body.velocity.x) > 0.2f || (phase == game::Phase::Gate && session.doorOpen(0) >= 0.0f)) {
        pose = std::abs(hero.body.velocity.x) > 0.2f ? 1 : 0;
        ph = gamelib::fx::walkPhase(hero.walked, 26.0f);
    }
    const float charge = hero.weapon == game::Weapon::Buster && hero.charge.heldTicks() > game::kChargeHalfTicks / 2
                             ? hero.charge.progress()
                             : 0.0f;
    draw(Rect::fromBottomCenter(feet + Vec2{0.0f, 3.0f}, drawSize + Vec2{6.0f, 6.0f}), pose, ph, hero.shootTicks > 0 && !hero.body.sliding,
         std::min(charge, 1.0f));
}

} // namespace blaster::app
