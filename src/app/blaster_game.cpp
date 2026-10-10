#include "blaster_game.hpp"

#include "palette.hpp"

#include <gamelib/app/settings.hpp>
#include <gamelib/core/easing.hpp>
#include <gamelib/core/timers.hpp>
#include <gamelib/draw/parallax.hpp>
#include <gamelib/draw/vector_text.hpp>
#include <gamelib/ui/menu.hpp>
#include <gamelib/ui/panel.hpp>

#include <algorithm>
#include <cstdlib>
#include <string_view>
#include <array>
#include <cmath>
#include <numbers>
#include <utility>

namespace blaster::app {

namespace {

// helpers

using gamelib::app::ArcadeAction;
using gamelib::app::ArcadeScreen;
using gamelib::core::Color;
using gamelib::core::Rect;
using gamelib::core::Vec2;
using gamelib::platform::GamepadButton;
using gamelib::platform::Key;

constexpr int kWeaponsCommand = 0; // the pause menu's WEAPONS
constexpr int kExitCommand = 1;    // the pause menu's EXIT STAGE
constexpr int kControlsCommand = 7;
constexpr int kTankRow = 100;
constexpr int kBackRow = 101;
constexpr float kEndingMenuDelay = 16.0f;
constexpr float kPi = std::numbers::pi_v<float>;

gamelib::ui::MenuStyle _menuStyle()
{
    return {.veil = Color{0.01f, 0.01f, 0.04f, 0.55f},
            .panel = srgb(0x12132A, 0.97f),
            .border = srgb(0x5FE0FF),
            .title = srgb(0x5FE0FF),
            .text = kText,
            .highlight = srgb(0xFFE14D),
            .selection = srgb(0x23305A),
            .dim = kTextDim,
            .titlePixel = 11.0f,
            .itemPixel = 5.0f,
            .itemHeight = 60.0f,
            .padding = 44.0f,
            .glowWidth = 24.0f};
}

game::Session _demoSession()
{
    return game::Session{game::StageId::Intro, game::Carry::fresh(9, 0, 0)};
}

std::optional<BlasterGame::Command> _command(const std::optional<gamelib::ui::MenuInput> input)
{
    using C = BlasterGame::Command;
    if (!input) {
        return std::nullopt;
    }
    switch (*input) {
    case gamelib::ui::MenuInput::Up: return C::Up;
    case gamelib::ui::MenuInput::Down: return C::Down;
    case gamelib::ui::MenuInput::Left: return C::Left;
    case gamelib::ui::MenuInput::Right: return C::Right;
    case gamelib::ui::MenuInput::Confirm: return C::Confirm;
    case gamelib::ui::MenuInput::Back: return C::Back;
    }
    return std::nullopt;
}

std::string _stageTitle(const game::StageId id)
{
    return std::string{game::stage(id).name};
}

Vec2 _panelCenter(const int index)
{
    const int col = index % 3;
    const int row = index / 3;
    return {960.0f + static_cast<float>(col - 1) * 440.0f, 330.0f + static_cast<float>(row) * 330.0f};
}

} // namespace

BlasterGame::BlasterGame(gamelib::app::App& app, ProfileFile profile, const bool demo)
    : m_profile{std::move(profile)}
    , m_menus{"BLASTER", m_profile.value().settings, {}, {.music = true, .vsync = true, .controls = kControlsCommand},
              [](gamelib::ui::Menu& paused) {
                  paused.action("WEAPONS", kWeaponsCommand);
                  paused.action("EXIT STAGE", kExitCommand);
              }}
    , m_sfx{app.audio(), sfxRecipe}
    , m_music{app.audio(), [](const Tune t) { return tune(t); }}
    , m_shapes{Shapes::registerWith(app.renderer())}
    , m_canvas{app.canvas()}
    , m_view{app.canvas()}
    , m_dev{gamelib::app::readDevStart("BLASTER")}
    , m_devStage{m_dev.level && *m_dev.level < game::kStageCount ? *m_dev.level : -1}
    , m_attract{[](std::uint64_t) { return DemoRun{.session = _demoSession(), .bot = {}}; },
                [](DemoRun& run, float) {
                    run.session.tick(run.bot.next(run.session));
                    if (run.session.phase() != game::Phase::Over || run.session.outcome() == game::Outcome::Cleared) {
                        return;
                    }
                    // Died: again from the checkpoint.
                    game::Carry carry = run.session.carry();
                    carry.lives = 9;
                    run.session = game::Session{game::StageId::Intro, carry};
                    run.bot = {};
                    ++run.attempts;
                    ++run.generation;
                },
                [](const DemoRun& run) {
                    return run.attempts >= 4
                           || (run.session.phase() == game::Phase::Over && run.session.outcome() == game::Outcome::Cleared);
                },
                1}
    , m_demoView{app.canvas()}
    , m_demoMode{demo}
{
    m_menus.setSounds({.moved = gamelib::app::MenuSound{m_sfx.id(Sfx::MenuMove)},
                       .changed = gamelib::app::MenuSound{m_sfx.id(Sfx::MenuMove)},
                       .selected = gamelib::app::MenuSound{m_sfx.id(Sfx::MenuSelect)},
                       .back = gamelib::app::MenuSound{m_sfx.id(Sfx::MenuBack)}});
    m_profile.save(); // a missing file is written with the defaults, so it can be found and edited
    gamelib::app::applySettings(app, m_profile.value().settings);
    m_music.play(app.audio(), Tune::Title, 0.6f);
}

void BlasterGame::save()
{
    m_profile.save();
}

void BlasterGame::onFocus(gamelib::app::App& app, const bool focused)
{
    if (!m_demoMode) {
        m_menus.onFocus(app, focused, m_time);
    }
}

void BlasterGame::onKey(gamelib::app::App& app, const gamelib::platform::KeyEvent& event)
{
    if (event.pressed && event.key == Key::F3) {
        m_debug.toggle();
        return;
    }
    if (m_demoMode) {
        if (event.pressed && event.key != Key::F11) {
            m_demoMode = false; // any key leaves the full-screen demo for the title
            app.input().releaseAll();
        }
        return;
    }
    if (m_overlay == Overlay::Controls && m_controls) {
        const gamelib::ui::BindingList::Result r = m_controls->handle(event);
        gamelib::app::playMenuSound(app.sounds(), m_menus.sounds(), r.feedback);
        if (r.changed) {
            save();
        }
        if (r.closed) {
            m_overlay = Overlay::None;
            m_controls.reset();
            app.input().releaseAll();
        }
        return;
    }
    if (m_overlay == Overlay::Weapons) {
        if (event.pressed) {
            std::optional<Command> c = _command(gamelib::ui::menuInput(event.key));
            const Action a = actionOf(progress().actions, event.key);
            if (!c && (a == Action::Jump || a == Action::Shoot)) {
                c = Command::Confirm;
            }
            if (!c && a == Action::Pause) {
                c = Command::Back;
            }
            if (c) {
                weaponsCommand(app, *c);
            }
        }
        return;
    }
    const gamelib::app::ArcadeResult result = m_menus.onKey(app, event, m_time);
    handleResult(app, result);
    if (!event.pressed || result.from != ArcadeScreen::Playing || !m_menus.is(ArcadeScreen::Playing)) {
        return;
    }
    std::optional<Command> c = _command(gamelib::ui::menuInput(event.key));
    const Action a = actionOf(progress().actions, event.key);
    switch (a) {
    case Action::Up: c = Command::Up; break;
    case Action::Down: c = Command::Down; break;
    case Action::Left: c = Command::Left; break;
    case Action::Right: c = Command::Right; break;
    case Action::Jump:
    case Action::Shoot: c = Command::Confirm; break;
    default: break;
    }
    if (c) {
        command(app, *c);
    }
}

void BlasterGame::onGamepad(gamelib::app::App& app, const gamelib::platform::GamepadEvent& event)
{
    if (m_demoMode) {
        if (event.pressed) {
            m_demoMode = false;
            app.input().releaseAll();
        }
        return;
    }
    if (m_overlay == Overlay::Controls && m_controls) {
        const gamelib::ui::BindingList::Result r = m_controls->handle(event);
        gamelib::app::playMenuSound(app.sounds(), m_menus.sounds(), r.feedback);
        if (r.changed) {
            save();
        }
        if (r.closed) {
            m_overlay = Overlay::None;
            m_controls.reset();
            app.input().releaseAll();
        }
        return;
    }
    if (m_overlay == Overlay::Weapons) {
        if (event.pressed) {
            std::optional<Command> c = _command(gamelib::ui::menuInput(event.button));
            if (event.button == GamepadButton::Start) {
                c = Command::Back;
            }
            if (c) {
                weaponsCommand(app, *c);
            }
        }
        return;
    }
    const gamelib::app::ArcadeResult result = m_menus.onGamepad(app, event, m_time);
    handleResult(app, result);
    if (!event.pressed || result.from != ArcadeScreen::Playing || !m_menus.is(ArcadeScreen::Playing)) {
        return;
    }
    if (const std::optional<Command> c = _command(gamelib::ui::menuInput(event.button))) {
        command(app, *c);
    } else if (event.button == GamepadButton::X) {
        command(app, Command::Confirm);
    }
}

void BlasterGame::handleResult(gamelib::app::App& app, const gamelib::app::ArcadeResult& result)
{
    if (result.changed) {
        save();
    }
    switch (result.action) {
    case ArcadeAction::Start:
        if (result.from == ArcadeScreen::Paused) {
            // RESTART: the stage from its beginning.
            if (m_mode == Mode::Stage && m_session && !m_transition.active()) {
                game::Carry again = m_stageStart;
                again.lives = m_lives;
                again.tanks = m_session->tanks();
                startStage(app, m_stage, again, false);
            }
            break;
        }
        m_lives = game::kStartLives;
        m_gameOver = false;
        if (const char* scene = std::getenv("BLASTER_SCENE"); scene != nullptr && result.from == ArcadeScreen::Title) {
            // Developer aid: straight to the weapon-get scene or the ending.
            if (std::string_view{scene} == "ending") {
                startEnding(app);
            } else {
                startWeaponGet(app, static_cast<game::BossKind>(std::clamp(std::atoi(scene), 0, game::kMasters - 1)));
            }
        } else if (m_devStage >= 0 && result.from == ArcadeScreen::Title) {
            startStage(app, static_cast<game::StageId>(m_devStage), game::Carry::fresh(m_lives, progress().tanks, 0x3F), true);
        } else if (!progress().introCleared) {
            startStage(app, game::StageId::Intro, game::Carry::fresh(m_lives, progress().tanks, progress().weapons()), true);
        } else {
            enterSelect(app);
        }
        break;
    case ArcadeAction::ToTitle:
        if (m_session) {
            progress().tanks = m_session->tanks();
        }
        m_session.reset();
        m_transition.cancel();
        m_card.reset();
        m_mode = Mode::Select;
        m_overlay = Overlay::None;
        m_gameOver = false;
        save();
        m_music.play(app.audio(), Tune::Title, 0.6f);
        break;
    case ArcadeAction::Quit:
        if (m_session) {
            progress().tanks = m_session->tanks();
        }
        save();
        app.requestQuit();
        break;
    case ArcadeAction::Controls: openControls(app); break;
    case ArcadeAction::Game:
        if (result.command == kWeaponsCommand && m_mode == Mode::Stage && m_session) {
            openWeapons(app);
        } else if (result.command == kExitCommand && m_mode == Mode::Stage && m_session && !m_transition.active()
                   && progress().introCleared && m_session->phase() != game::Phase::BossDown) {
            leaveStage(app);
        }
        break;
    case ArcadeAction::Resume:
    case ArcadeAction::None: break;
    }
}

void BlasterGame::openControls(gamelib::app::App& app)
{
    m_controls.emplace(progress().actions, "CONTROLS");
    m_overlay = Overlay::Controls;
    app.input().releaseAll();
}

std::vector<int> BlasterGame::weaponRows() const
{
    std::vector<int> rows;
    for (int w = 0; w < game::kWeaponCount; ++w) {
        if (m_session && m_session->owns(static_cast<game::Weapon>(w))) {
            rows.push_back(w);
        }
    }
    rows.push_back(kTankRow);
    rows.push_back(kBackRow);
    return rows;
}

void BlasterGame::openWeapons(gamelib::app::App& app)
{
    m_overlay = Overlay::Weapons;
    const std::vector<int> rows = weaponRows();
    m_weaponCursor = 0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (m_session && rows[i] == static_cast<int>(m_session->hero().weapon)) {
            m_weaponCursor = static_cast<int>(i);
        }
    }
    app.input().releaseAll();
}

void BlasterGame::weaponsCommand(gamelib::app::App& app, const Command c)
{
    const std::vector<int> rows = weaponRows();
    const int count = static_cast<int>(rows.size());
    const auto close = [&] {
        m_overlay = Overlay::None;
        app.input().releaseAll();
    };
    switch (c) {
    case Command::Up:
        m_weaponCursor = (m_weaponCursor + count - 1) % count;
        m_sfx.queue(app.sounds(), Sfx::MenuMove);
        break;
    case Command::Down:
        m_weaponCursor = (m_weaponCursor + 1) % count;
        m_sfx.queue(app.sounds(), Sfx::MenuMove);
        break;
    case Command::Confirm: {
        const int row = rows[static_cast<std::size_t>(std::clamp(m_weaponCursor, 0, count - 1))];
        if (row == kBackRow) {
            m_sfx.queue(app.sounds(), Sfx::MenuBack);
            close();
            m_menus.go(ArcadeScreen::Paused, m_time);
        } else if (row == kTankRow) {
            if (m_session && m_session->useTank()) {
                progress().tanks = m_session->tanks();
                ++progress().tanksUsed;
                save();
                for (const game::Event& e : m_session->events().take()) {
                    react(app, e);
                }
            } else {
                m_sfx.queue(app.sounds(), Sfx::Deflect);
            }
        } else if (m_session) {
            m_session->selectWeapon(static_cast<game::Weapon>(row));
            for (const game::Event& e : m_session->events().take()) {
                react(app, e);
            }
            m_sfx.queue(app.sounds(), Sfx::MenuSelect);
            close();
        }
        break;
    }
    case Command::Back:
        m_sfx.queue(app.sounds(), Sfx::MenuBack);
        close();
        break;
    default: break;
    }
}

void BlasterGame::command(gamelib::app::App& app, const Command c)
{
    if (m_transition.active()) {
        return;
    }
    switch (m_mode) {
    case Mode::Select: selectCommand(app, c); break;
    case Mode::WeaponGet:
        if (m_sceneDone && (c == Command::Confirm || c == Command::Back)) {
            m_sfx.queue(app.sounds(), Sfx::MenuSelect);
            enterSelect(app);
        }
        break;
    case Mode::Stage:
    case Mode::Ending: break;
    }
}

void BlasterGame::selectCommand(gamelib::app::App& app, const Command c)
{
    const int before = m_cursor;
    switch (c) {
    case Command::Left:
        if (m_cursor < 6) {
            m_cursor = m_cursor / 3 * 3 + (m_cursor % 3 + 2) % 3;
        }
        break;
    case Command::Right:
        if (m_cursor < 6) {
            m_cursor = m_cursor / 3 * 3 + (m_cursor % 3 + 1) % 3;
        }
        break;
    case Command::Up: m_cursor = m_cursor == 6 ? 4 : (m_cursor >= 3 ? m_cursor - 3 : 6); break;
    case Command::Down: m_cursor = m_cursor == 6 ? 1 : (m_cursor < 3 ? m_cursor + 3 : 6); break;
    case Command::Confirm: {
        const game::Carry carry = game::Carry::fresh(m_lives, progress().tanks, progress().weapons());
        if (m_cursor < 6) {
            m_sfx.queue(app.sounds(), Sfx::MenuSelect);
            startStage(app, game::stageOf(static_cast<game::BossKind>(m_cursor)), carry, true);
        } else if (progress().fortressOpen()) {
            m_sfx.queue(app.sounds(), Sfx::MenuSelect);
            startStage(app, game::fortressStage(std::min(progress().fortress, game::kFortressStages - 1)), carry, true);
        } else {
            m_sfx.queue(app.sounds(), Sfx::Deflect);
        }
        return;
    }
    case Command::Back:
    case Command::None: break;
    }
    if (m_cursor != before) {
        m_sfx.queue(app.sounds(), Sfx::MenuMove);
    }
}

void BlasterGame::enterSelect(gamelib::app::App& app)
{
    m_transition.start({.kind = gamelib::ui::TransitionKind::Fade, .color = kInk, .coverSeconds = 0.35f, .holdSeconds = 0.1f,
                        .uncoverSeconds = 0.35f},
                       [this, &app] {
                           m_mode = Mode::Select;
                           m_session.reset();
                           m_card.reset();
                           m_overlay = Overlay::None;
                           m_music.play(app.audio(), Tune::Select, 0.3f);
                           save();
                       });
}

void BlasterGame::startStage(gamelib::app::App& app, const game::StageId stage, const game::Carry& carry, const bool fromSelect)
{
    m_stage = stage;
    if (fromSelect) {
        m_stageStart = carry; // what RESTART starts from
    }
    const game::StageDef& def = game::stage(stage);
    std::string line;
    if (def.boss != game::BossKind::Count) {
        line = std::string{game::bossName(def.boss)};
    } else if (stage == game::StageId::Intro) {
        line = "THE OVERSEER STIRS";
    } else {
        line = "THE OVERSEER'S FORTRESS";
    }
    if (carry.checkpoint >= 0) {
        line = "FROM THE CHECKPOINT";
    }
    m_card = Card{.title = _stageTitle(stage), .line = line};
    m_charging = false;
    m_sfx.stopLoop(app.audio(), Sfx::Charge);
    m_transition.start({.kind = fromSelect ? gamelib::ui::TransitionKind::Iris : gamelib::ui::TransitionKind::Fade,
                        .color = kInk, .coverSeconds = fromSelect ? 0.5f : 0.35f, .holdSeconds = fromSelect ? 1.6f : 0.9f,
                        .uncoverSeconds = 0.4f},
                       [this, &app, stage, carry] {
                           m_mode = Mode::Stage;
                           m_session.emplace(stage, carry);
                           m_replay = static_cast<int>(stage) == m_devStage ? m_dev.playback<game::Input>(game::decodeInput)
                                                                            : gamelib::core::Playback<game::Input>{};
                           if (m_dev.column && static_cast<int>(stage) == m_devStage && carry.checkpoint < 0) {
                               m_session->debugStartAt(*m_dev.column, 2);
                           }
                           m_view.snap(*m_session);
                           m_effects.clear();
                           m_gameOver = false;
                           m_cleared = false;
                           m_lastHealth = m_session->hero().health.current;
                           m_healthTrail.reset(1.0f);
                           m_music.stop(app.audio(), 0.2f);
                           m_sfx.queue(app.sounds(), Sfx::Ready);
                       });
}

void BlasterGame::leaveStage(gamelib::app::App& app)
{
    if (m_session) {
        progress().tanks = m_session->tanks();
        progress().destroyed += m_session->enemiesDestroyed();
    }
    m_music.stop(app.audio(), 0.3f);
    enterSelect(app);
}

void BlasterGame::tick(gamelib::app::App& app)
{
    if (m_menus.is(ArcadeScreen::Title) || m_demoMode) {
        demoTick();
    }
    if (!m_menus.is(ArcadeScreen::Playing) || m_demoMode || m_overlay != Overlay::None) {
        return;
    }
    if (m_mode == Mode::Stage) {
        stageTick(app);
    }
}

void BlasterGame::stageTick(gamelib::app::App& app)
{
    if (!m_session || m_transition.active() || m_gameOver || m_cleared) {
        return;
    }
    const std::optional<game::Input> replayed = m_replay.next();
    m_session->tick(replayed ? *replayed : readInput(progress().actions, app.input()));
    ++progress().playTicks;
    for (const game::Event& event : m_session->events().take()) {
        react(app, event);
    }
    // The ladder's footsteps.
    const game::Hero& hero = m_session->hero();
    if (hero.body.climbing && std::abs(hero.body.box.y - hero.previous.y) > 0.1f && m_session->ticks() % 9 == 0) {
        m_sfx.queue(app.sounds(), Sfx::Ladder);
    }
    if (m_session->phase() == game::Phase::Over) {
        attemptOver(app);
    }
}

void BlasterGame::demoTick()
{
    m_attract.tick(1.0f / static_cast<float>(game::kTickRate));
    DemoRun& run = m_attract.sim();
    const int generation = run.generation + 1000 * m_attract.restarts();
    if (generation != m_demoGeneration) {
        m_demoGeneration = generation;
        m_demoView.snap(run.session);
        m_demoEffects.clear();
    }
    for (const game::Event& event : run.session.events().take()) {
        m_demoEffects.onEvent(event, run.session.def().theme);
    }
}

void BlasterGame::react(gamelib::app::App& app, const game::Event& e)
{
    using game::EventKind;
    if (!m_session) {
        return;
    }
    m_effects.onEvent(e, m_session->def().theme);
    auto& sounds = app.sounds();
    switch (e.kind) {
    case EventKind::Shoot:
        if (e.weapon == game::Weapon::Buster) {
            m_sfx.queue(sounds, e.value == 2 ? Sfx::ShootFull : (e.value == 1 ? Sfx::ShootHalf : Sfx::Shoot));
        } else {
            m_sfx.queue(sounds, Sfx::Weapon, 1.0f, 0.9f + 0.05f * static_cast<float>(e.weapon));
        }
        break;
    case EventKind::ChargeReady: m_sfx.queue(sounds, Sfx::ChargeReady); break;
    case EventKind::Land: m_sfx.queue(sounds, Sfx::Land); break;
    case EventKind::Slide: m_sfx.queue(sounds, Sfx::Slide); break;
    case EventKind::HitEnemy: m_sfx.queue(sounds, Sfx::EnemyHit); break;
    case EventKind::Deflect: m_sfx.queue(sounds, Sfx::Deflect); break;
    case EventKind::EnemyDown:
        m_sfx.queue(sounds, Sfx::Explosion, e.value == 1 ? 1.0f : 0.8f, e.value == 1 ? 0.75f : 1.0f);
        if (e.value == 1) {
            app.hitStop(0.12f);
        }
        break;
    case EventKind::EnemyShot: m_sfx.queue(sounds, Sfx::EnemyShot, 0.7f); break;
    case EventKind::Bomb: m_sfx.queue(sounds, Sfx::Bomb); break;
    case EventKind::Hurt: m_sfx.queue(sounds, Sfx::Hurt); break;
    case EventKind::Death:
        m_sfx.queue(sounds, Sfx::Death);
        m_music.stop(app.audio(), 0.1f);
        app.hitStop(0.15f);
        break;
    case EventKind::Pickup:
        switch (e.pickup) {
        case game::PickupKind::Tank: m_sfx.queue(sounds, Sfx::Tank); break;
        case game::PickupKind::ExtraLife: m_sfx.queue(sounds, Sfx::ExtraLife); break;
        default: m_sfx.queue(sounds, Sfx::Pickup); break;
        }
        break;
    case EventKind::Checkpoint: m_sfx.queue(sounds, Sfx::Checkpoint); break;
    case EventKind::DoorOpen:
    case EventKind::DoorClose: m_sfx.queue(sounds, Sfx::DoorOpen); break;
    case EventKind::Unlock: m_sfx.queue(sounds, Sfx::Unlock); break;
    case EventKind::BossIntro:
        m_banners.add(std::string{game::bossName(e.boss)}, "", bossColors(e.boss).trim, 2.2f);
        break;
    case EventKind::BossFill: m_sfx.queue(sounds, Sfx::BossFill); break;
    case EventKind::BossHit:
        m_sfx.queue(sounds, Sfx::BossHit);
        app.hitStop(0.05f);
        break;
    case EventKind::BossPhase: m_sfx.queue(sounds, Sfx::BossLand, 1.0f, 1.3f); break;
    case EventKind::BossDown:
        m_sfx.queue(sounds, Sfx::BossDown);
        m_music.stop(app.audio(), 0.1f);
        app.hitStop(0.35f);
        break;
    case EventKind::BossLand: m_sfx.queue(sounds, Sfx::BossLand); break;
    case EventKind::Strike: m_sfx.queue(sounds, Sfx::Strike); break;
    case EventKind::WeaponSwitch: m_sfx.queue(sounds, Sfx::WeaponSwitch); break;
    case EventKind::Tank: m_sfx.queue(sounds, Sfx::Tank); break;
    case EventKind::BeamIn:
    case EventKind::BeamOut: m_sfx.queue(sounds, Sfx::Beam); break;
    case EventKind::Jump:
    case EventKind::Scroll:
    case EventKind::Clear: break;
    }
}

void BlasterGame::attemptOver(gamelib::app::App& app)
{
    Progress& p = progress();
    p.destroyed += m_session->enemiesDestroyed();
    p.tanks = m_session->tanks();
    if (m_session->outcome() == game::Outcome::Died) {
        ++p.deaths;
        game::Carry carry = m_session->carry();
        m_lives = carry.lives;
        if (m_lives < 0) {
            // Game over: back to the select with fresh lives; progress stays.
            m_gameOver = true;
            m_lives = game::kStartLives;
            m_sfx.queue(app.sounds(), Sfx::GameOver);
            m_menus.setGameOverTitle("GAME OVER");
            m_menus.gameOverAfter(1.2f, m_time);
            save();
            return;
        }
        save();
        startStage(app, m_stage, carry, false);
        return;
    }
    m_cleared = true;
    stageCleared(app);
}

void BlasterGame::stageCleared(gamelib::app::App& app)
{
    Progress& p = progress();
    const game::StageDef& def = game::stage(m_stage);
    switch (m_stage) {
    case game::StageId::Intro:
        p.introCleared = true;
        save();
        m_banners.add("STAGE SELECT", "SIX ROBOT MASTERS AWAIT", kGold, 2.5f);
        enterSelect(app);
        break;
    case game::StageId::Fortress1:
    case game::StageId::Fortress2:
        p.fortress = std::max(p.fortress, static_cast<int>(m_stage) - static_cast<int>(game::StageId::Fortress1) + 1);
        save();
        startStage(app, static_cast<game::StageId>(static_cast<int>(m_stage) + 1),
                   game::Carry::fresh(m_lives, p.tanks, p.weapons()), true);
        break;
    case game::StageId::Fortress3:
        p.fortress = game::kFortressStages;
        p.finished = true;
        (void)m_profile.value().bests.offer(kBestTime, p.playTicks, gamelib::app::Bests::Better::Lower);
        save();
        startEnding(app);
        break;
    default:
        p.beaten = static_cast<std::uint8_t>(p.beaten | (1u << static_cast<int>(def.boss)));
        save();
        startWeaponGet(app, def.boss);
        break;
    }
}

void BlasterGame::startWeaponGet(gamelib::app::App& app, const game::BossKind boss)
{
    m_transition.start({.kind = gamelib::ui::TransitionKind::Fade, .color = kInk, .coverSeconds = 0.4f, .holdSeconds = 0.2f,
                        .uncoverSeconds = 0.4f},
                       [this, &app, boss] {
                           m_mode = Mode::WeaponGet;
                           m_session.reset();
                           m_got = boss;
                           m_sceneTime = 0.0f;
                           m_sceneFlash = 0.0f;
                           m_sceneText = false;
                           m_sceneDone = false;
                           m_effects.clear();
                           m_music.play(app.audio(), Tune::WeaponGet, 0.2f);
                           m_scene.clear();
                           m_scene.wait(0.9f)
                               .then([this, &app] { m_sfx.queue(app.sounds(), Sfx::Beam); })
                               .during(1.8f, [this](const float t) { m_sceneFlash = t; })
                               .then([this, &app] {
                                   m_sceneText = true;
                                   m_sfx.queue(app.sounds(), Sfx::WeaponGet);
                                   m_effects.burst({960.0f, 520.0f}, heroColors(game::weaponOf(m_got)).trim, 160.0f);
                               })
                               .wait(1.6f)
                               .then([this] { m_sceneDone = true; });
                       });
}

void BlasterGame::startEnding(gamelib::app::App& app)
{
    m_transition.start({.kind = gamelib::ui::TransitionKind::Fade, .color = kInk, .coverSeconds = 0.8f, .holdSeconds = 0.4f,
                        .uncoverSeconds = 0.8f},
                       [this, &app] {
                           m_mode = Mode::Ending;
                           m_session.reset();
                           m_sceneTime = 0.0f;
                           m_effects.clear();
                           m_music.play(app.audio(), Tune::Ending, 0.5f);
                           m_menus.setGameOverTitle("THE END");
                           m_menus.gameOverAfter(kEndingMenuDelay, m_time);
                       });
}

void BlasterGame::frame(gamelib::app::App& app, const gamelib::app::FrameInfo& info)
{
    m_time = info.time;
    m_transition.update(info.seconds);
    if (!m_transition.active()) {
        m_card.reset();
    }
    m_effects.update(info.seconds);
    m_demoEffects.update(info.seconds);
    m_banners.update(info.seconds);
    m_debug.frameTime(info.seconds);
    if (m_mode == Mode::WeaponGet || m_mode == Mode::Ending) {
        m_sceneTime += info.seconds;
        if (m_mode == Mode::WeaponGet) {
            m_scene.update(info.seconds);
        }
        if (m_mode == Mode::Ending && std::fmod(m_sceneTime, 0.7f) < info.seconds && m_sceneTime > 2.0f) {
            static constexpr std::array<std::uint32_t, 5> kColors{0xFFE14D, 0xFF6A3D, 0x5CE1FF, 0x7CFF6B, 0xFF5AB8};
            const int i = static_cast<int>(m_sceneTime * 3.0f) % 5;
            const Vec2 at{960.0f + std::sin(m_sceneTime * 2.7f) * 600.0f, 250.0f + std::cos(m_sceneTime * 1.9f) * 80.0f};
            m_effects.burst(at, srgb(kColors[static_cast<std::size_t>(i)]).scaled(1.4f), 70.0f);
        }
    }
    if (m_menus.openGameOverWhenDue(app, m_time)) {
        save();
    }
    if (m_session && m_mode == Mode::Stage) {
        const bool frozen = m_transition.active() || !m_menus.is(ArcadeScreen::Playing) || m_overlay != Overlay::None;
        m_view.follow(*m_session, frozen ? 1.0f : info.alpha, info.seconds);
        const int health = m_session->hero().health.current;
        m_healthTrail.set(static_cast<float>(health) / static_cast<float>(game::kMaxHealth));
        m_healthTrail.update(info.seconds);
        m_lastHealth = health;
    }
    if (m_menus.is(ArcadeScreen::Title) || m_demoMode) {
        m_demoView.follow(m_attract.sim().session, info.alpha, info.seconds);
    }
    updateMusic(app);
    updateLoops(app);
}

void BlasterGame::updateMusic(gamelib::app::App& app)
{
    if (m_menus.is(ArcadeScreen::Title) || m_demoMode) {
        m_music.play(app.audio(), Tune::Title, 0.6f);
        return;
    }
    if (m_transition.active()) {
        return;
    }
    switch (m_mode) {
    case Mode::Select: m_music.play(app.audio(), Tune::Select, 0.4f); return;
    case Mode::WeaponGet: m_music.play(app.audio(), Tune::WeaponGet, 0.3f); return;
    case Mode::Ending: m_music.play(app.audio(), Tune::Ending, 0.5f); return;
    case Mode::Stage: break;
    }
    if (!m_session) {
        return;
    }
    const game::Phase phase = m_session->phase();
    if (phase == game::Phase::Dying || phase == game::Phase::Over || phase == game::Phase::BossDown) {
        return;
    }
    const game::Boss* boss = m_session->activeBoss();
    if (boss != nullptr && !boss->sm.is(game::BossState::Dead)) {
        m_music.play(app.audio(), boss->kind == game::BossKind::Overseer ? Tune::FinalBoss : Tune::Boss, 0.3f);
        return;
    }
    if (phase == game::Phase::Gate) {
        return; // the door's rumble alone
    }
    m_music.play(app.audio(), stageTune(m_session->def().theme), 0.5f);
}

void BlasterGame::updateLoops(gamelib::app::App& app)
{
    bool charging = false;
    float level = 0.0f;
    if (m_session && m_mode == Mode::Stage && m_menus.is(ArcadeScreen::Playing) && m_overlay == Overlay::None
        && m_session->phase() == game::Phase::Playing && m_session->hero().weapon == game::Weapon::Buster) {
        const gamelib::core::ChargeMeter& charge = m_session->hero().charge;
        charging = charge.heldTicks() > game::kChargeHalfTicks / 2;
        level = charge.progress();
    }
    if (charging) {
        m_sfx.loop(app.audio(), Sfx::Charge, 0.55f, 0.8f + 0.7f * level);
    } else if (m_charging) {
        m_sfx.stopLoop(app.audio(), Sfx::Charge);
    }
    m_charging = charging;
}

bool BlasterGame::showsStage() const
{
    return m_session && m_mode == Mode::Stage && !m_demoMode && !m_menus.is(ArcadeScreen::Title);
}

void BlasterGame::draw(gamelib::app::App& app, gamelib::draw::DrawList& list, const gamelib::app::FrameInfo& info)
{
    (void)app;
    const bool shakeOn = m_profile.value().settings.screenShake;
    list.setPostProcess({.bloom = {.intensity = 0.45f, .threshold = 0.8f}});
    if (showsStage()) {
        const bool frozen = !m_menus.is(ArcadeScreen::Playing) || m_transition.active() || m_overlay != Overlay::None;
        const Vec2 shake = shakeOn ? m_effects.shake(info.time) : Vec2{};
        m_view.draw(list, m_shapes, *m_session, m_effects,
                    {.alpha = frozen ? 1.0f : info.alpha, .shake = shake, .actions = &progress().actions});
        list.setView({});
        list.setLayer(gamelib::draw::layers::Hud);
        m_effects.drawFlash(list, m_canvas);
        drawHud(list, *m_session);
        m_banners.draw(list, static_cast<float>(m_canvas.width) / 2.0f,
                       {.top = 300.0f, .titleSize = 80.0f, .lineColor = kText, .shadow = kInk.withAlpha(0.6f)});
        if (m_overlay == Overlay::Weapons) {
            drawWeapons(list);
        }
        drawDebug(list, *m_session);
    } else if (m_menus.is(ArcadeScreen::Title) || m_demoMode) {
        const DemoRun& run = m_attract.sim();
        m_demoView.draw(list, m_shapes, run.session, m_demoEffects, {.alpha = info.alpha, .actions = &progress().actions});
        list.setView({});
        list.setLayer(gamelib::draw::layers::Hud);
        if (m_demoMode) {
            drawHud(list, run.session);
            list.text("DEMO", static_cast<float>(m_canvas.width) / 2.0f, static_cast<float>(m_canvas.height) - 80.0f,
                      {.pixelSize = 6.0f, .color = kText.withAlpha(0.6f + 0.4f * std::sin(info.time * 3.0f)),
                       .align = gamelib::draw::Align::Center, .shadow = true});
        }
    } else if (m_mode == Mode::WeaponGet) {
        drawWeaponGet(list, info.time);
    } else if (m_mode == Mode::Ending) {
        drawEnding(list, info.time);
    } else {
        drawSelect(list, info.time);
        list.setLayer(gamelib::draw::layers::Hud);
        m_banners.draw(list, static_cast<float>(m_canvas.width) / 2.0f,
                       {.top = 420.0f, .titleSize = 80.0f, .lineColor = kText, .shadow = kInk.withAlpha(0.6f),
                        .panel = kPanel.withAlpha(0.92f)});
    }
    list.setView({});
    list.setLayer(gamelib::draw::layers::Hud + 10);
    m_transition.draw(list, m_canvas);
    drawCard(list);
    if (!m_demoMode) {
        drawMenus(list);
        if (m_overlay == Overlay::Controls && m_controls) {
            list.setLayer(gamelib::draw::layers::Hud + 20);
            gamelib::ui::drawBindingList(list, *m_controls, m_canvas, _menuStyle(), 11);
        }
    }
}

void BlasterGame::drawHud(gamelib::draw::DrawList& list, const game::Session& session) const
{
    const game::Hero& hero = session.hero();
    // The health bar: 28 units, Mega Man style, top left.
    const Rect health{56.0f, 60.0f, 34.0f, 330.0f};
    gamelib::ui::drawSegmentBar(list, health, game::kMaxHealth, hero.health.current,
                                {.background = srgb(0x07060F, 0.9f), .filled = kHealth, .empty = srgb(0x2A2F45),
                                 .gap = 3.0f, .inset = 4.0f, .cornerRadius = 1.5f, .from = gamelib::ui::FillFrom::Bottom});
    if (hero.weapon != game::Weapon::Buster) {
        const HeroColors c = heroColors(hero.weapon);
        const int units = (hero.energy[static_cast<std::size_t>(hero.weapon)].current + game::kEnergyPerUnit - 1)
                          / game::kEnergyPerUnit;
        gamelib::ui::drawSegmentBar(list, {100.0f, 60.0f, 34.0f, 330.0f}, game::kMaxEnergy, units,
                                    {.background = srgb(0x07060F, 0.9f), .filled = c.trim, .empty = srgb(0x2A2F45),
                                     .gap = 3.0f, .inset = 4.0f, .cornerRadius = 1.5f,
                                     .from = gamelib::ui::FillFrom::Bottom});
        list.text(std::string{game::weaponInfo(hero.weapon).shortName}, 117.0f, 398.0f,
                  {.pixelSize = 3.0f, .color = c.trim, .align = gamelib::draw::Align::Center, .shadow = true});
    }
    // Lives and tanks, on a dark plate so they read on any backdrop.
    list.box({40.0f, 426.0f, 112.0f, session.tanks() > 0 ? 120.0f : 64.0f}, Color{0.02f, 0.02f, 0.06f, 0.55f}, {.cornerRadius = 10.0f});
    list.add({.rect = {48.0f, 432.0f, 52.0f, 52.0f}, .color = heroColors(hero.weapon).main, .color2 = heroColors(hero.weapon).trim,
              .shape = m_shapes.item, .params = {4.0f, 0.0f, 0.0f, 2.0f}, .scale = 3.4f});
    list.text("x" + std::to_string(std::max(session.lives(), 0)), 104.0f, 446.0f,
              {.pixelSize = 4.0f, .color = kText, .shadow = true});
    if (session.tanks() > 0) {
        list.add({.rect = {52.0f, 492.0f, 46.0f, 46.0f}, .color = srgb(0x2F7BFF), .color2 = srgb(0x6FE0FF), .shape = m_shapes.item,
                  .params = {5.0f, 0.0f, 0.0f, 2.0f}, .scale = 2.8f});
        list.text("x" + std::to_string(session.tanks()), 104.0f, 504.0f, {.pixelSize = 4.0f, .color = kText, .shadow = true});
    }
    // The boss's bar, top right.
    if (const game::Boss* boss = session.activeBoss(); boss != nullptr && !boss->sm.is(game::BossState::Dead)) {
        gamelib::ui::drawSegmentBar(list, {static_cast<float>(m_canvas.width) - 90.0f, 60.0f, 34.0f, 330.0f}, game::kBossHealth,
                                    boss->shown,
                                    {.background = srgb(0x07060F, 0.9f), .filled = bossColors(boss->kind).trim,
                                     .empty = srgb(0x2A2F45), .gap = 3.0f, .inset = 4.0f, .cornerRadius = 1.5f,
                                     .from = gamelib::ui::FillFrom::Bottom});
    }
    // A guardian's bar while it lives.
    for (const auto [handle, e] : session.enemies()) {
        if (e.kind == game::EnemyKind::MiniBoss) {
            gamelib::ui::drawBar(list, {static_cast<float>(m_canvas.width) / 2.0f - 240.0f, 60.0f, 480.0f, 26.0f},
                                 static_cast<float>(e.hp) / static_cast<float>(e.maxHp),
                                 {.fill = srgb(0xFF8A3A), .cornerRadius = 6.0f});
        }
    }
    // "READY" before the hero beams in.
    if (session.phase() == game::Phase::Ready && session.phaseTicks() < game::kReadyTicks - 18 && (session.phaseTicks() / 10) % 2 == 0) {
        gamelib::draw::drawVectorText(list, "READY", static_cast<float>(m_canvas.width) / 2.0f, 470.0f,
                                      {.size = 84.0f, .color = kText, .weight = 1.6f, .align = gamelib::draw::Align::Center});
    }
}

void BlasterGame::drawSelect(gamelib::draw::DrawList& list, const float time) const
{
    const float w = static_cast<float>(m_canvas.width);
    const float h = static_cast<float>(m_canvas.height);
    list.setLayer(gamelib::draw::layers::Background);
    list.vignette({0.0f, 0.0f, w, h}, srgb(0x10183A), 1.4f, 0.3f);
    // Moving stripes behind the panels.
    for (int i = 0; i < 14; ++i) {
        const float x = std::fmod(static_cast<float>(i) * 160.0f + time * 40.0f, w + 320.0f) - 160.0f;
        list.box({x, 0.0f, 50.0f, h}, srgb(0x1C2A5A, 0.35f), {}, 0.25f);
    }
    list.setLayer(gamelib::draw::layers::Entities);
    gamelib::draw::drawVectorText(list, "STAGE SELECT", w / 2.0f, 60.0f,
                                  {.size = 72.0f, .color = srgb(0x5FE0FF), .weight = 1.6f, .align = gamelib::draw::Align::Center});
    const Progress& p = progress();
    for (int i = 0; i < game::kMasters; ++i) {
        const auto boss = static_cast<game::BossKind>(i);
        const Vec2 c = _panelCenter(i);
        const bool selected = m_cursor == i;
        const bool beaten = p.hasBeaten(boss);
        const HeroColors bc = bossColors(boss);
        const float pulse = 0.5f + 0.5f * std::sin(time * 6.0f);
        list.box(Rect::fromCenter(c, {360.0f, 260.0f}), kPanel.withAlpha(0.95f),
                 {.cornerRadius = 18.0f, .borderWidth = selected ? 6.0f : 3.0f,
                  .borderColor = selected ? srgb(0xFFE14D) : srgb(0x3A4A86), .glowWidth = selected ? 18.0f + 10.0f * pulse : 0.0f,
                  .glowColor = srgb(0xFFE14D, 0.5f)});
        list.box(Rect::fromCenter(c + Vec2{0.0f, -16.0f}, {300.0f, 190.0f}),
                 themePalette(game::stage(game::stageOf(boss)).theme)[Role::SkyTop].scaled(0.8f), {.cornerRadius = 10.0f});
        StageView::drawBoss(list, m_shapes, c + Vec2{0.0f, -14.0f}, boss, 1, selected ? 1 : 0, 3.4f, beaten ? 0.45f : 1.0f);
        list.text(std::string{game::bossName(boss)}, c.x, c.y + 96.0f,
                  {.pixelSize = 4.0f, .color = beaten ? kTextDim : bc.trim, .align = gamelib::draw::Align::Center, .shadow = true});
        if (beaten) {
            list.box(Rect::fromCenter(c + Vec2{0.0f, -20.0f}, {200.0f, 54.0f}), srgb(0x07060F, 0.7f),
                     {.cornerRadius = 8.0f, .borderWidth = 3.0f, .borderColor = kGold}, -0.2f);
            list.text("CLEAR", c.x, c.y - 34.0f, {.pixelSize = 5.0f, .color = kGold, .align = gamelib::draw::Align::Center});
        }
    }
    // The fortress.
    const bool open = p.fortressOpen();
    const bool selected = m_cursor == 6;
    const Vec2 fc{w / 2.0f, 960.0f};
    list.box(Rect::fromCenter(fc, {1100.0f, 110.0f}), (open ? srgb(0x2A1430) : kPanel).withAlpha(0.95f),
             {.cornerRadius = 18.0f, .borderWidth = selected ? 6.0f : 3.0f,
              .borderColor = selected ? srgb(0xFFE14D) : (open ? srgb(0xFF3A4A) : srgb(0x3A4A86)),
              .glowWidth = selected ? 20.0f : 0.0f, .glowColor = srgb(0xFFE14D, 0.5f)});
    std::string fortress = open ? "THE OVERSEER'S FORTRESS" : "FORTRESS LOCKED - BEAT ALL SIX";
    if (open && p.fortress > 0) {
        fortress += "  " + std::to_string(p.fortress) + "/3";
    }
    list.text(fortress, fc.x, fc.y - 14.0f,
              {.pixelSize = 5.0f, .color = open ? srgb(0xFF6A7A) : kTextDim, .align = gamelib::draw::Align::Center, .shadow = true});
    const std::string footer = "LIVES " + std::to_string(m_lives) + "    TANKS " + std::to_string(p.tanks) + "    WEAPONS "
                               + std::to_string(p.beatenCount()) + "/6";
    list.text(footer, w / 2.0f, 1040.0f, {.pixelSize = 3.0f, .color = kTextDim, .align = gamelib::draw::Align::Center});
}

void BlasterGame::drawWeapons(gamelib::draw::DrawList& list) const
{
    if (!m_session) {
        return;
    }
    list.setLayer(gamelib::draw::layers::Hud + 5);
    const std::vector<int> rows = weaponRows();
    const float w = static_cast<float>(m_canvas.width);
    const float rowH = 74.0f;
    const float height = 230.0f + rowH * static_cast<float>(rows.size());
    const Rect panel = Rect::fromCenter({w / 2.0f, 540.0f}, {980.0f, height});
    list.box({0.0f, 0.0f, w, static_cast<float>(m_canvas.height)}, Color{0.0f, 0.0f, 0.03f, 0.55f});
    list.box(panel, srgb(0x12132A, 0.97f), {.cornerRadius = 22.0f, .borderWidth = 4.0f, .borderColor = srgb(0x5FE0FF),
                                            .glowWidth = 24.0f, .glowColor = srgb(0x5FE0FF, 0.35f)});
    list.text("WEAPONS", w / 2.0f, panel.y + 34.0f, {.pixelSize = 10.0f, .color = srgb(0x5FE0FF), .align = gamelib::draw::Align::Center});
    const game::Hero& hero = m_session->hero();
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const float y = panel.y + 130.0f + rowH * static_cast<float>(i);
        const bool selected = static_cast<int>(i) == m_weaponCursor;
        if (selected) {
            list.box({panel.x + 30.0f, y - 8.0f, panel.width - 60.0f, rowH - 10.0f}, srgb(0x23305A), {.cornerRadius = 10.0f});
        }
        const Color text = selected ? srgb(0xFFE14D) : kText;
        const int row = rows[i];
        if (row == kTankRow) {
            list.add({.rect = {panel.x + 60.0f, y - 2.0f, 50.0f, 50.0f}, .color = srgb(0x2F7BFF), .color2 = srgb(0x6FE0FF),
                      .shape = m_shapes.item, .params = {5.0f, 0.0f, 0.0f, 2.0f}, .scale = 3.0f});
            list.text("ENERGY TANK  x" + std::to_string(m_session->tanks()), panel.x + 140.0f, y + 10.0f,
                      {.pixelSize = 5.0f, .color = m_session->tanks() > 0 ? text : kTextDim});
            continue;
        }
        if (row == kBackRow) {
            list.text("BACK", panel.x + 140.0f, y + 10.0f, {.pixelSize = 5.0f, .color = text});
            continue;
        }
        const auto weapon = static_cast<game::Weapon>(row);
        const HeroColors c = heroColors(weapon);
        list.add({.rect = {panel.x + 60.0f, y - 2.0f, 50.0f, 50.0f}, .color = c.main, .color2 = c.trim, .shape = m_shapes.item,
                  .params = {7.0f, 0.0f, static_cast<float>(row), 2.0f}, .scale = 3.0f});
        list.text(std::string{game::weaponInfo(weapon).name} + (weapon == hero.weapon ? "  <" : ""), panel.x + 140.0f, y + 10.0f,
                  {.pixelSize = 5.0f, .color = text});
        if (weapon != game::Weapon::Buster) {
            const int units = (hero.energy[static_cast<std::size_t>(row)].current + game::kEnergyPerUnit - 1) / game::kEnergyPerUnit;
            gamelib::ui::drawSegmentBar(list, {panel.x + 520.0f, y + 2.0f, 400.0f, 36.0f}, game::kMaxEnergy, units,
                                        {.filled = c.trim, .gap = 2.0f, .inset = 3.0f, .from = gamelib::ui::FillFrom::Left});
        }
    }
    list.text("HEALTH", panel.x + 140.0f, panel.bottom() - 56.0f, {.pixelSize = 4.0f, .color = kTextDim});
    gamelib::ui::drawSegmentBar(list, {panel.x + 520.0f, panel.bottom() - 62.0f, 400.0f, 36.0f}, game::kMaxHealth,
                                hero.health.current, {.filled = kHealth, .gap = 2.0f, .inset = 3.0f, .from = gamelib::ui::FillFrom::Left});
}

void BlasterGame::drawWeaponGet(gamelib::draw::DrawList& list, const float time) const
{
    const float w = static_cast<float>(m_canvas.width);
    const float h = static_cast<float>(m_canvas.height);
    const game::Weapon weapon = game::weaponOf(m_got);
    const HeroColors c = heroColors(weapon);
    list.setLayer(gamelib::draw::layers::Background);
    list.vignette({0.0f, 0.0f, w, h}, srgb(0x0A0A20), 1.2f, 0.2f);
    // Rays turning behind the hero.
    for (int i = 0; i < 12; ++i) {
        const float a = static_cast<float>(i) * kPi / 6.0f + time * 0.2f;
        list.box(Rect::fromCenter({w / 2.0f, 520.0f}, {2200.0f, 18.0f}), c.trim.withAlpha(0.08f + 0.12f * m_sceneFlash),
                 {.glowWidth = 14.0f, .glowColor = c.trim.withAlpha(0.06f + 0.08f * m_sceneFlash)}, a);
    }
    list.setLayer(gamelib::draw::layers::Entities);
    // The hero takes on the new colors: flickering between the old and the new.
    const bool newColors = m_sceneFlash >= 1.0f || (m_sceneFlash > 0.0f && static_cast<int>(time * 14.0f) % 2 == 0);
    StageView::drawHero(list, m_shapes, Rect::fromCenter({w / 2.0f, 520.0f}, {320.0f, 320.0f}),
                        newColors ? weapon : game::Weapon::Buster, 1, m_sceneText ? 0 : 0, 0.0f, m_sceneText, 0.0f, 9.5f);
    list.setLayer(gamelib::draw::layers::Effects);
    m_effects.drawWorld(list);
    list.setLayer(gamelib::draw::layers::Hud);
    gamelib::draw::drawVectorText(list, "WEAPON GET", w / 2.0f, 120.0f,
                                  {.size = 80.0f, .color = kText, .weight = 1.6f, .align = gamelib::draw::Align::Center});
    if (m_sceneText) {
        const std::string name = "YOU GOT " + std::string{game::weaponInfo(weapon).name};
        gamelib::draw::drawVectorText(list, name, w / 2.0f, 760.0f,
                                      {.size = 64.0f, .color = c.trim, .weight = 1.4f, .align = gamelib::draw::Align::Center});
        // Whose weakness it is.
        for (int b = 0; b < game::kMasters; ++b) {
            if (game::weaknessOf(static_cast<game::BossKind>(b)) == weapon) {
                list.text(std::string{game::bossName(static_cast<game::BossKind>(b))} + " FEARS IT", w / 2.0f, 860.0f,
                          {.pixelSize = 4.0f, .color = kTextDim, .align = gamelib::draw::Align::Center, .shadow = true});
            }
        }
    }
    if (m_sceneDone && std::fmod(time, 1.0f) < 0.7f) {
        list.text("PRESS " + withKeys("{jump}", progress().actions), w / 2.0f, 980.0f,
                  {.pixelSize = 4.0f, .color = kText, .align = gamelib::draw::Align::Center});
    }
}

void BlasterGame::drawEnding(gamelib::draw::DrawList& list, const float time) const
{
    const float w = static_cast<float>(m_canvas.width);
    const float h = static_cast<float>(m_canvas.height);
    const ThemePalette& pal = themePalette(game::Theme::Lab);
    list.setLayer(gamelib::draw::layers::Background);
    // The lab's skyline at dawn, scrolling past as the hero walks home.
    const gamelib::core::View2D view{.origin = {m_sceneTime * 60.0f, 0.0f}, .zoom = kZoom};
    const std::array layers{
        gamelib::draw::ParallaxLayer{.shape = m_shapes.backdrop, .factor = {0.05f, 0.0f}, .color = srgb(0x2A3A7A),
                                     .color2 = srgb(0xFFB070), .params = {0.0f, 0.0f}},
        gamelib::draw::ParallaxLayer{.shape = m_shapes.backdrop, .factor = {0.15f, 0.0f}, .color = pal[Role::Far],
                                     .color2 = pal[Role::FarLight], .params = {0.0f, 1.0f}},
        gamelib::draw::ParallaxLayer{.shape = m_shapes.backdrop, .factor = {0.35f, 0.0f}, .color = pal[Role::Near],
                                     .color2 = pal[Role::NearLight], .params = {0.0f, 2.0f}},
    };
    gamelib::draw::drawParallax(list, layers, view, m_canvas);
    list.setLayer(gamelib::draw::layers::TilesBehind);
    list.box({0.0f, 900.0f, w, h - 900.0f}, pal[Role::Ground]);
    list.box({0.0f, 900.0f, w, 10.0f}, pal[Role::GroundEdge]);
    list.setLayer(gamelib::draw::layers::Entities);
    StageView::drawHero(list, m_shapes, Rect::fromBottomCenter({w * 0.3f, 914.0f}, {210.0f, 210.0f}), game::Weapon::Buster, 1, 1,
                        std::fmod(m_sceneTime * 1.6f, 1.0f), false, 0.0f, 6.5f);
    list.setLayer(gamelib::draw::layers::Effects);
    m_effects.drawWorld(list);
    list.setLayer(gamelib::draw::layers::Hud);
    if (m_menus.is(ArcadeScreen::GameOver)) {
        return;
    }
    const Progress& p = progress();
    const float a = gamelib::core::smoothstep(0.5f, 1.5f, m_sceneTime);
    gamelib::draw::drawVectorText(list, "The Overseer has fallen.", w / 2.0f, 110.0f,
                                  {.size = 64.0f, .color = kGold.withAlpha(a), .weight = 1.4f, .align = gamelib::draw::Align::Center});
    const std::array<std::string, 5> lines{
        "TIME   " + gamelib::core::formatClock(p.playTicks, game::kTickRate, 0),
        "LIVES LOST   " + std::to_string(p.deaths),
        "ROBOTS DESTROYED   " + std::to_string(p.destroyed),
        "ENERGY TANKS USED   " + std::to_string(p.tanksUsed),
        "BEST TIME   " + gamelib::core::formatClock(m_profile.value().bests.get(kBestTime).value_or(p.playTicks), game::kTickRate, 0),
    };
    const Rect panel{w * 0.5f, 250.0f, 820.0f, 420.0f};
    list.box(panel, kPanel.withAlpha(0.8f * gamelib::core::smoothstep(1.5f, 2.5f, m_sceneTime)),
             {.cornerRadius = 22.0f, .borderWidth = 3.0f, .borderColor = srgb(0x5FE0FF, gamelib::core::smoothstep(1.5f, 2.5f, m_sceneTime))});
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const float fade = gamelib::core::smoothstep(2.0f + 0.6f * static_cast<float>(i), 2.8f + 0.6f * static_cast<float>(i), m_sceneTime);
        list.text(lines[i], panel.x + 50.0f, panel.y + 50.0f + 72.0f * static_cast<float>(i),
                  {.pixelSize = 5.0f, .color = kText.withAlpha(fade), .shadow = true});
    }
    const float thanks = gamelib::core::smoothstep(6.0f, 7.0f, m_sceneTime);
    gamelib::draw::drawVectorText(list, "Thanks for playing Blaster!", w * 0.5f + 410.0f, 720.0f,
                                  {.size = 44.0f, .color = kText.withAlpha(thanks), .weight = 1.2f, .align = gamelib::draw::Align::Center});
    (void)time;
}

void BlasterGame::drawCard(gamelib::draw::DrawList& list) const
{
    const float a = m_transition.cardAlpha(0.2f);
    if (!m_card || a <= 0.0f) {
        return;
    }
    const float cx = static_cast<float>(m_canvas.width) / 2.0f;
    const float cy = static_cast<float>(m_canvas.height) / 2.0f;
    gamelib::draw::drawVectorText(list, m_card->title, cx, cy - 90.0f,
                                  {.size = 66.0f, .color = kText.withAlpha(a), .weight = 1.4f, .align = gamelib::draw::Align::Center});
    if (!m_card->line.empty()) {
        list.text(m_card->line, cx, cy + 40.0f, {.pixelSize = 5.0f, .color = srgb(0x5FE0FF, a), .align = gamelib::draw::Align::Center});
    }
    list.text("LIVES x" + std::to_string(std::max(m_lives, 0)), cx, cy + 130.0f,
              {.pixelSize = 4.0f, .color = kTextDim.withAlpha(a), .align = gamelib::draw::Align::Center});
}

void BlasterGame::drawMenus(gamelib::draw::DrawList& list) const
{
    const gamelib::ui::Menu* menu = m_menus.menu();
    if (menu == nullptr || m_overlay == Overlay::Controls) {
        return;
    }
    std::string subtitle;
    std::string footer;
    const Progress& p = progress();
    if (m_menus.is(ArcadeScreen::Title)) {
        subtitle = "ROBOT MASTERS BEATEN " + std::to_string(p.beatenCount()) + "/6";
        if (p.finished) {
            subtitle += "  THE OVERSEER IS DOWN";
        }
        footer = withKeys("{left}{right} MOVE  {jump} JUMP  {shoot} SHOOT  {slide} SLIDE  {weapon_prev}{weapon_next} WEAPON",
                          p.actions);
    } else if (m_menus.is(ArcadeScreen::GameOver)) {
        subtitle = m_mode == Mode::Ending ? "TIME " + gamelib::core::formatClock(p.playTicks, game::kTickRate, 0)
                                          : "YOUR PROGRESS IS SAVED";
    } else if (m_menus.is(ArcadeScreen::Paused)) {
        if (m_session && m_mode == Mode::Stage) {
            subtitle = std::string{m_session->def().name};
        }
    }
    gamelib::ui::drawMenu(list, *menu, m_canvas, _menuStyle(), subtitle, footer);
}

void BlasterGame::drawDebug(gamelib::draw::DrawList& list, const game::Session& session)
{
    if (!m_debug.enabled()) {
        m_debug.draw(list);
        return;
    }
    const gamelib::core::View2D v = m_view.view();
    m_debug.box(v.rectToCanvas(session.hero().body.box), Color{0.3f, 1.0f, 0.4f, 1.0f});
    for (const auto [handle, e] : session.enemies()) {
        m_debug.box(v.rectToCanvas(e.body.box), e.vulnerable.guarded() ? Color{0.6f, 0.6f, 1.0f, 1.0f} : Color{1.0f, 0.3f, 0.3f, 1.0f});
    }
    if (const game::Boss* boss = session.activeBoss()) {
        m_debug.box(v.rectToCanvas(boss->body.box), Color{1.0f, 0.5f, 0.1f, 1.0f});
    }
    for (const game::Shot& s : session.shots().items()) {
        m_debug.box(v.rectToCanvas(s.box()), Color{1.0f, 1.0f, 0.3f, 1.0f});
    }
    const auto& hero = session.hero().body;
    m_debug.stat("STAGE", std::string{session.def().id} + " ROOM " + std::to_string(session.roomIndex()));
    m_debug.stat("POS", std::to_string(static_cast<int>(hero.box.x)) + "," + std::to_string(static_cast<int>(hero.box.y)));
    m_debug.stat("PHASE", std::to_string(static_cast<int>(session.phase())) + " TICK " + std::to_string(session.ticks()));
    m_debug.draw(list);
}

} // namespace blaster::app
