// Blaster - generated from GameLib's new-game template.
//
// A glowing orb you steer with WASD or the arrow keys: bump the edges of the
// canvas as often as you can in 30 seconds. Space changes its color with a
// chime. Everything on screen is drawn by shaders: GameLib's built-in
// vignette, circle, menus and pixel-font text, and this game's own
// shaders/orb.frag.
//
// GameLib's standard arcade pieces are already wired up: the title, pause and
// game-over menus (app::ArcadeMenus, Escape or P pauses, F11 is fullscreen)
// and the profile file with the settings and the best score
// (app::ArcadeProfile, blaster.ini in the user data folder;
// BLASTER_DATA_DIR moves it).
//
// Command line (GameLib self-test switches): --frames N --hidden
// --require-validation --no-vsync --resize-at-frame N --minimize-at-frame N
#include "orb_frag.spv.hpp"

#include <gamelib/app/app.hpp>
#include <gamelib/app/arcade_menus.hpp>
#include <gamelib/app/arcade_profile.hpp>
#include <gamelib/audio/synth.hpp>
#include <gamelib/core/color.hpp>
#include <gamelib/core/math.hpp>
#include <gamelib/core/score.hpp>
#include <gamelib/fx/juice.hpp>
#include <gamelib/ui/menu.hpp>

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <utility>

namespace {

using gamelib::app::ArcadeAction;
using gamelib::app::ArcadeScreen;
using gamelib::core::Color;
using gamelib::core::Vec2;
using gamelib::platform::Key;

constexpr Vec2 kCanvas{1920.0f, 1080.0f};
constexpr float kOrbRadius = 70.0f;
constexpr float kOrbGlow = 40.0f;
constexpr float kMaxSpeed = 950.0f;      // canvas units per second
constexpr float kAcceleration = 4000.0f; // units per second squared
constexpr float kRoundSeconds = 30.0f;

const std::array kColors{gamelib::core::srgb(0x31C7EF), gamelib::core::srgb(0xFF5E8A), gamelib::core::srgb(0xFFD166),
                         gamelib::core::srgb(0x7BE07B)};

class OrbGame final : public gamelib::app::Game {
public:
    OrbGame(gamelib::app::App& app, gamelib::app::ArcadeProfileFile profile)
        : m_profile{std::move(profile)}
        , m_orbShape{app.renderer().addShape(shaders::orb_frag)}
    {
        // Sounds are synthesized once at startup; no audio files.
        using gamelib::audio::Wave;
        using gamelib::audio::note;
        const gamelib::audio::SoundRecipe bump{{{gamelib::audio::tone(Wave::Triangle, note("A2"), 0.08f, note("D2"))},
                                                {gamelib::audio::noise(3000.0f, 0.03f, 0.0f, 0.4f)}},
                                               -14.0f};
        const gamelib::audio::SoundRecipe chime{{{gamelib::audio::tone(Wave::Pulse25, note("C5"), 0.06f),
                                                 gamelib::audio::tone(Wave::Pulse25, note("G5"), 0.06f),
                                                 gamelib::audio::tone(Wave::Pulse25, note("C6"), 0.16f)}},
                                                -15.0f};
        m_bumpSound = app.audio().add(bump.render(app.audio().sampleRate()));
        m_chimeSound = app.audio().add(chime.render(app.audio().sampleRate()));
        m_menus.setSounds({.moved = gamelib::app::MenuSound{m_bumpSound, 0.6f, 1.6f},
                           .selected = gamelib::app::MenuSound{m_chimeSound},
                           .back = gamelib::app::MenuSound{m_bumpSound, 0.6f, 1.6f}});
        m_profile.save(); // a missing file is written with the defaults, so it can be found and edited
        m_scores.loadBest(m_profile.value().bests.get(gamelib::app::kScore).value_or(0));
        gamelib::app::applySettings(app, m_profile.value().settings);
    }

    // Presses that matter once (not held) arrive here, as they happen. The
    // menus see every key first; what is left while playing is the game's.
    void onKey(gamelib::app::App& app, const gamelib::platform::KeyEvent& event) override
    {
        const gamelib::app::ArcadeResult result = m_menus.onKey(app, event, m_time);
        if (result.changed) {
            m_profile.save();
        }
        switch (result.action) {
        case ArcadeAction::Start: startRound(); break;
        case ArcadeAction::ToTitle: saveBest(); break;
        case ArcadeAction::Quit:
            saveBest();
            app.requestQuit();
            break;
        case ArcadeAction::Resume:
        case ArcadeAction::Controls: // only with SettingsItems::controls
        case ArcadeAction::Game:     // only with pause-menu actions of the game's own
        case ArcadeAction::None: break;
        }
        if (result.action == ArcadeAction::None && m_menus.is(ArcadeScreen::Playing) && event.pressed
            && event.key == Key::Space) {
            m_colorIndex = (m_colorIndex + 1) % kColors.size();
            m_pulse = 1.0f;
            app.sounds().queue(m_chimeSound);
        }
    }

    // The simulation: a fixed step, reading the held keys. Only while playing.
    void tick(gamelib::app::App& app) override
    {
        m_previous = m_position;
        if (!m_menus.is(ArcadeScreen::Playing)) {
            return;
        }
        const float dt = app.tickSeconds();
        const gamelib::platform::InputState& keys = app.input();
        const Vec2 wanted = gamelib::core::normalized(
                                {static_cast<float>(keys.axis({Key::A, Key::Left}, {Key::D, Key::Right})),
                                 static_cast<float>(keys.axis({Key::W, Key::Up}, {Key::S, Key::Down}))})
                            * kMaxSpeed;
        const float step = kAcceleration * dt;
        m_velocity = {gamelib::core::approach(m_velocity.x, wanted.x, step),
                      gamelib::core::approach(m_velocity.y, wanted.y, step)};
        m_position += m_velocity * dt;

        // Bounce off the canvas edges, with a sound and a point for each new contact.
        bool bumped = false;
        if (m_position.x < kOrbRadius || m_position.x > kCanvas.x - kOrbRadius) {
            m_position.x = std::clamp(m_position.x, kOrbRadius, kCanvas.x - kOrbRadius);
            m_velocity.x = -m_velocity.x * 0.8f;
            bumped = true;
        }
        if (m_position.y < kOrbRadius || m_position.y > kCanvas.y - kOrbRadius) {
            m_position.y = std::clamp(m_position.y, kOrbRadius, kCanvas.y - kOrbRadius);
            m_velocity.y = -m_velocity.y * 0.8f;
            bumped = true;
        }
        if (bumped && gamelib::core::length(m_velocity) > 120.0f) {
            m_scores.add(1);
            m_shake.trigger(0.5f);
            app.sounds().queue(m_bumpSound, 1.0f, 0.9f + 0.2f * (m_position.y / kCanvas.y));
        }

        m_roundLeft = std::max(0.0f, m_roundLeft - dt);
        if (m_roundLeft == 0.0f) {
            saveBest();
            m_menus.openGameOver(app, m_time);
        }
    }

    // Presentation on real time: effects that fade regardless of tick rate.
    void frame(gamelib::app::App&, const gamelib::app::FrameInfo& info) override
    {
        m_time = info.time;
        m_pulse = std::max(0.0f, m_pulse - info.seconds * 2.5f);
        m_shake.update(info.seconds);
    }

    void draw(gamelib::app::App& app, gamelib::draw::DrawList& list, const gamelib::app::FrameInfo& info) override
    {
        list.vignette({0.0f, 0.0f, kCanvas.x, kCanvas.y}, gamelib::core::srgb(0x141A30));

        // Interpolate between the last two ticks for smooth motion at any refresh rate.
        // The screen-shake setting decides whether bumps shake the view.
        const Vec2 shake = m_profile.value().settings.screenShake ? m_shake.offset(info.time) : Vec2{};
        const Vec2 orb = gamelib::core::lerp(m_previous, m_position, info.alpha) + shake;
        const Color color = kColors[m_colorIndex];
        list.circle(orb + Vec2{0.0f, kOrbRadius * 0.9f}, kOrbRadius * 0.8f, {0.0f, 0.0f, 0.0f, 0.25f},
                    {.glowWidth = 30.0f, .glowColor = {0.0f, 0.0f, 0.0f, 0.2f}}); // a soft shadow
        const float size = 2.0f * (kOrbRadius + kOrbGlow) * (1.0f + 0.15f * m_pulse);
        list.add({
            .rect = gamelib::core::Rect::fromCenter(orb, {size, size}),
            .color = color,
            .color2 = gamelib::core::lerp(color, Color{1.0f, 1.0f, 1.0f, 1.0f}, 0.4f),
            .shape = m_orbShape,
            .params = {m_pulse, kOrbGlow, 0.0f, 0.0f},
        });

        const gamelib::core::Color text = gamelib::core::srgb(0xF2F0E5);
        list.text("BUMPS " + std::to_string(m_scores.score()), 60.0f, 60.0f, {.pixelSize = 5.0f, .color = text});
        list.text("TIME " + std::to_string(static_cast<int>(m_roundLeft + 0.99f)), kCanvas.x - 60.0f, 60.0f,
                  {.pixelSize = 5.0f, .color = text, .align = gamelib::draw::Align::Right});

        // The menus over the game: nothing while playing.
        const gamelib::ui::Menu* menu = m_menus.menu();
        if (menu == nullptr) {
            return;
        }
        std::string subtitle;
        std::string footer;
        if (m_menus.is(ArcadeScreen::Title)) {
            subtitle = m_scores.best() > 0 ? "BEST " + std::to_string(m_scores.best()) : "";
            footer = "MOVE: WASD / ARROWS    COLOR: SPACE    PAUSE: ESC    FULLSCREEN: F11";
        } else if (m_menus.is(ArcadeScreen::GameOver)) {
            subtitle = (m_scores.isNewBest() ? "NEW BEST! " : "") + std::string{"BUMPS "}
                       + std::to_string(m_scores.score());
        }
        gamelib::ui::drawMenu(list, *menu, app.canvas(), {}, subtitle, footer);
    }

private:
    void startRound()
    {
        m_scores.reset();
        m_roundLeft = kRoundSeconds;
        m_position = m_previous = kCanvas / 2.0f;
        m_velocity = {};
        m_shake.clear();
    }

    void saveBest()
    {
        m_profile.value().bests.offer(gamelib::app::kScore, m_scores.best());
        m_profile.save();
    }

    gamelib::app::ArcadeProfileFile m_profile; // the menus edit its settings in place
    gamelib::app::ArcadeMenus m_menus{"BLASTER", m_profile.value().settings};
    gamelib::core::ScoreKeeper m_scores;
    gamelib::draw::ShapeId m_orbShape;
    gamelib::audio::SoundId m_bumpSound{};
    gamelib::audio::SoundId m_chimeSound{};
    Vec2 m_position = kCanvas / 2.0f;
    Vec2 m_previous = m_position;
    Vec2 m_velocity;
    std::size_t m_colorIndex = 0;
    gamelib::fx::Shake m_shake;
    float m_pulse = 0.0f;
    float m_roundLeft = kRoundSeconds;
    float m_time = 0.0f;
};

} // namespace

int main(int argc, char** argv)
{
    // Loaded before the window exists, so a saved fullscreen setting is
    // applied while the window is still hidden.
    gamelib::app::ArcadeProfileFile profile = gamelib::app::openArcadeProfile("Blaster", "BLASTER_DATA_DIR");
    const gamelib::app::AppConfig config{
        .title = "Blaster",
        .windowSize = {1280, 720},
        .canvas = {1920, 1080},
        .tick = std::chrono::nanoseconds{1'000'000'000 / 120},
        .fullscreen = profile.value().settings.fullscreen,
        .f11TogglesFullscreen = false, // F11 goes through ArcadeMenus, so the setting is saved
    };
    return gamelib::app::runMain(argc, argv, config, [&](gamelib::app::App& app) {
        return std::make_unique<OrbGame>(app, std::move(profile));
    });
}
