#pragma once

// Blaster on GameLib's main loop: the title (the demo bot playing the
// intro behind the menu), the stage select, the stages with a card and
// "READY", pause with the weapon menu and the controls screen (GameLib's
// BindingList over the ActionMap), the weapon-get scene after each robot
// master, game over, and the ending after the Overseer. The rules live in
// src/game; this is input, presentation, sound and saves.

#include "actions.hpp"
#include "effects.hpp"
#include "profile.hpp"
#include "sounds.hpp"
#include "view.hpp"

#include "demo_bot.hpp"
#include "session.hpp"

#include <gamelib/ai/attract.hpp>
#include <gamelib/app/app.hpp>
#include <gamelib/app/arcade_menus.hpp>
#include <gamelib/app/dev_start.hpp>
#include <gamelib/core/animation.hpp>
#include <gamelib/core/replay.hpp>
#include <gamelib/fx/banners.hpp>
#include <gamelib/fx/debug_overlay.hpp>
#include <gamelib/ui/binding_list.hpp>
#include <gamelib/ui/hud.hpp>
#include <gamelib/ui/transition.hpp>

#include <optional>
#include <string>
#include <vector>

namespace blaster::app {

class BlasterGame final : public gamelib::app::Game {
public:
    // demo: the demo plays full screen without the menu (--demo).
    BlasterGame(gamelib::app::App& app, ProfileFile profile, bool demo);

    void onFocus(gamelib::app::App& app, bool focused) override;
    void onKey(gamelib::app::App& app, const gamelib::platform::KeyEvent& event) override;
    void onGamepad(gamelib::app::App& app, const gamelib::platform::GamepadEvent& event) override;
    void tick(gamelib::app::App& app) override;
    void frame(gamelib::app::App& app, const gamelib::app::FrameInfo& info) override;
    void draw(gamelib::app::App& app, gamelib::draw::DrawList& list, const gamelib::app::FrameInfo& info) override;

    // What a menu-like screen (the select, the weapon menu) is told to do.
    enum class Command { None, Up, Down, Left, Right, Confirm, Back };

private:
    enum class Mode { Select, Stage, WeaponGet, Ending };
    enum class Overlay { None, Weapons, Controls };

    // The demo: the bot playing the intro stage.
    struct DemoRun {
        game::Session session;
        game::DemoBot bot;
        int attempts = 0;
        int generation = 0; // bumped on every new attempt, so the view snaps
    };
    struct Card {
        std::string title;
        std::string line;
    };

    void handleResult(gamelib::app::App& app, const gamelib::app::ArcadeResult& result);
    void command(gamelib::app::App& app, Command command);
    void selectCommand(gamelib::app::App& app, Command command);
    void weaponsCommand(gamelib::app::App& app, Command command);
    void openWeapons(gamelib::app::App& app);
    void openControls(gamelib::app::App& app);
    void enterSelect(gamelib::app::App& app);
    void startStage(gamelib::app::App& app, game::StageId stage, const game::Carry& carry, bool fromSelect);
    void stageTick(gamelib::app::App& app);
    void demoTick();
    void react(gamelib::app::App& app, const game::Event& event);
    void attemptOver(gamelib::app::App& app);
    void stageCleared(gamelib::app::App& app);
    void startWeaponGet(gamelib::app::App& app, game::BossKind boss);
    void startEnding(gamelib::app::App& app);
    void leaveStage(gamelib::app::App& app);
    void updateMusic(gamelib::app::App& app);
    void updateLoops(gamelib::app::App& app);
    void save();
    [[nodiscard]] Progress& progress() { return m_profile.value().game; }
    [[nodiscard]] const Progress& progress() const { return m_profile.value().game; }
    [[nodiscard]] bool showsStage() const;
    [[nodiscard]] std::vector<int> weaponRows() const;

    void drawHud(gamelib::draw::DrawList& list, const game::Session& session) const;
    void drawSelect(gamelib::draw::DrawList& list, float time) const;
    void drawWeapons(gamelib::draw::DrawList& list) const;
    void drawWeaponGet(gamelib::draw::DrawList& list, float time) const;
    void drawEnding(gamelib::draw::DrawList& list, float time) const;
    void drawCard(gamelib::draw::DrawList& list) const;
    void drawMenus(gamelib::draw::DrawList& list) const;
    void drawDebug(gamelib::draw::DrawList& list, const game::Session& session);

    ProfileFile m_profile; // the menus edit its settings in place: declared first
    gamelib::app::ArcadeMenus m_menus;
    Sounds m_sfx;
    Music m_music;
    Shapes m_shapes;
    gamelib::core::Size m_canvas;
    Mode m_mode = Mode::Select;
    Overlay m_overlay = Overlay::None;
    std::optional<gamelib::ui::BindingList> m_controls;
    int m_weaponCursor = 0;
    int m_cursor = 0; // the stage select: 0..5 the robot masters, 6 the fortress

    // The stage being played.
    std::optional<game::Session> m_session;
    game::StageId m_stage = game::StageId::Intro;
    game::Carry m_stageStart; // the carry the stage was entered with
    int m_lives = game::kStartLives;
    StageView m_view;
    Effects m_effects;
    gamelib::ui::Transition m_transition;
    std::optional<Card> m_card;
    bool m_gameOver = false;
    bool m_cleared = false;
    gamelib::ui::BarTrail m_healthTrail;
    int m_lastHealth = game::kMaxHealth;
    gamelib::app::DevStart m_dev;
    int m_devStage = -1;
    gamelib::core::Playback<game::Input> m_replay;
    bool m_charging = false;

    // The weapon-get scene and the ending.
    game::BossKind m_got = game::BossKind::Grinder;
    gamelib::core::Sequence m_scene;
    float m_sceneTime = 0.0f;
    float m_sceneFlash = 0.0f; // 0..1 while the hero takes on the new colors
    bool m_sceneText = false;
    bool m_sceneDone = false;

    // The demo behind the title.
    gamelib::ai::AttractMode<DemoRun> m_attract;
    StageView m_demoView;
    Effects m_demoEffects;
    int m_demoGeneration = -1;
    bool m_demoMode = false;

    gamelib::fx::Banners m_banners;
    gamelib::fx::DebugOverlay m_debug;
    float m_time = 0.0f;
};

} // namespace blaster::app
