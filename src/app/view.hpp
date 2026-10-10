#pragma once

// Drawing a stage: the camera (GameLib's Camera2D locked to the current
// room, gliding to the next one when the rules scroll), the parallax
// backdrop of the stage's theme, the tiles, boss doors, beacons, lifts,
// pickups, enemies, the boss, the shots, warnings of strikes to come, the
// hero and the weather in front. Presentation only.

#include "effects.hpp"
#include "session.hpp"

#include <gamelib/core/camera.hpp>
#include <gamelib/draw/draw_list.hpp>
#include <gamelib/gfx/renderer.hpp>
#include <gamelib/platform/actions.hpp>

namespace blaster::app {

// The game's own shader shapes, registered once.
struct Shapes {
    gamelib::draw::ShapeId tile;
    gamelib::draw::ShapeId hero;
    gamelib::draw::ShapeId enemy;
    gamelib::draw::ShapeId boss;
    gamelib::draw::ShapeId shot;
    gamelib::draw::ShapeId item;
    gamelib::draw::ShapeId backdrop;

    [[nodiscard]] static Shapes registerWith(gamelib::gfx::Renderer& renderer);
};

inline constexpr float kZoom = 4.5f; // canvas units per world unit: a 15-row room fills the 1080 canvas rows

struct StageDrawOptions {
    float alpha = 1.0f;        // interpolation between the last two ticks
    gamelib::core::Vec2 shake; // canvas units
    const gamelib::platform::ActionMap* actions = nullptr; // for the hints' key names
};

class StageView {
public:
    explicit StageView(gamelib::core::Size canvas);

    // Jumps the camera to the hero's room (a new attempt).
    void snap(const game::Session& session);
    // One frame of camera following (call before draw).
    void follow(const game::Session& session, float alpha, float seconds);
    void draw(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session, const Effects& effects,
              const StageDrawOptions& options) const;
    [[nodiscard]] gamelib::core::View2D view(gamelib::core::Vec2 shake = {}) const { return m_camera.view(shake); }

    // The hero as drawn on the HUD's cards and the weapon-get scene (canvas units).
    static void drawHero(gamelib::draw::DrawList& list, const Shapes& shapes, gamelib::core::Rect rect, game::Weapon weapon,
                         int facing, int pose, float phase, bool shooting, float charge, float scale);
    // A boss as drawn on its portrait.
    static void drawBoss(gamelib::draw::DrawList& list, const Shapes& shapes, gamelib::core::Vec2 center, game::BossKind boss,
                         int facing, int pose, float scale, float alpha = 1.0f);

private:
    void drawBackdrop(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session) const;
    void drawWeather(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session) const;
    void drawTiles(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session) const;
    void drawThings(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session, float alpha) const;
    void drawEnemies(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session, float alpha) const;
    void drawBosses(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session, float alpha) const;
    void drawShots(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session) const;
    void drawWarnings(gamelib::draw::DrawList& list, const game::Session& session) const;
    void drawHeroInStage(gamelib::draw::DrawList& list, const Shapes& shapes, const game::Session& session, float alpha) const;
    void drawHints(gamelib::draw::DrawList& list, const game::Session& session, const StageDrawOptions& options) const;

    gamelib::core::Size m_canvas;
    gamelib::core::Camera2D m_camera;
    int m_room = -1;
};

} // namespace blaster::app
