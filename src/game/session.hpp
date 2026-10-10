#pragma once

// One attempt at one stage: the hero (GameLib's platformer controller with
// a slide, the buster and its charge, the special weapons, ladders with
// shooting while hanging, knockback and invincibility), the rooms and the
// camera scrolls between them, checkpoints, the enemies and their spawn
// points (an enemy comes back when its point scrolls off screen and back),
// pickups, shots, boss doors, mini-bosses and the bosses. Deterministic:
// the same Carry and per-tick Input give the same attempt, so the demo bot
// and recordings replay exactly. No window, audio or clock.
//
// An attempt ends in Phase::Over with an Outcome: Cleared (the stage's last
// boss is beaten) or Died; the caller starts the next attempt with the
// Carry it returns (lives, tanks, the checkpoint, rematches won).

#include "rules.hpp"
#include "stages.hpp"
#include "weapons.hpp"

#include <gamelib/core/combat.hpp>
#include <gamelib/core/enemy_motion.hpp>
#include <gamelib/core/event_queue.hpp>
#include <gamelib/core/math.hpp>
#include <gamelib/core/platformer.hpp>
#include <gamelib/core/platforms.hpp>
#include <gamelib/core/projectiles.hpp>
#include <gamelib/core/rng.hpp>
#include <gamelib/core/skills.hpp>
#include <gamelib/core/slot_map.hpp>
#include <gamelib/core/spawner.hpp>
#include <gamelib/core/state_machine.hpp>
#include <gamelib/core/tile_collision.hpp>
#include <gamelib/core/tile_map.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace blaster::game {

// What goes from one attempt to the next (and from the app's save).
struct Carry {
    int lives = kStartLives;
    int tanks = 0;
    std::uint8_t weapons = 0; // the special weapons owned (bit i = Weapon i + 1)
    int checkpoint = -1;      // start at this checkpoint (-1: the stage's start)
    std::uint8_t rematches = 0; // bosses (bit = BossKind) already beaten in this stage attempt
    // Weapon energy (quarter units) per weapon; kept through deaths in a stage.
    std::array<gamelib::core::Resource, kWeaponCount> energy{};
    Weapon weapon = Weapon::Buster;

    [[nodiscard]] static Carry fresh(int lives, int tanks, std::uint8_t weapons);
};

enum class Phase {
    Ready,     // "READY", then the hero beams in
    Playing,
    Scroll,    // the camera scrolls to the next room; everything else holds
    Gate,      // a boss door opens, the hero walks through, the screen scrolls, it closes
    BossIntro, // the boss lands, poses and its health bar fills
    BossDown,  // the stage's last boss exploded: a pause, then the hero beams out
    Dying,
    Over,      // the attempt ended (see outcome())
};

enum class Outcome { None, Cleared, Died };

enum class EnemyKind : std::uint8_t {
    Crawler,      // walks its platform
    Hopper,       // hops towards the hero
    Diver,        // hovers, then dives at the hero
    ShieldWalker, // shielded while walking; opens to fire
    Turret,       // closed (shielded) cannon that opens to fire aimed shots
    Nest,         // releases drones
    Drone,        // a small flier that homes slowly
    Roller,       // rolls at the hero once it sees him
    Lobber,       // throws arcing bombs
    Popper,       // hides in the floor, pops up with a three-way spread
    Wisp,         // flies at the hero on a sine wave
    Dropper,      // hangs from the ceiling and drops on the hero
    MiniBoss,     // the stage's guardian
    Count
};
inline constexpr int kEnemyKinds = static_cast<int>(EnemyKind::Count);

struct Enemy {
    EnemyKind kind = EnemyKind::Crawler;
    gamelib::core::WalkerBody body;
    gamelib::core::Rect previous;
    gamelib::core::Vec2 origin;
    int point = -1; // the room's spawn point it came from (-1: released by a nest)
    int hp = 1;
    int maxHp = 1;
    int timer = 0;  // ticks in the current state
    int state = 0;  // the kind's own state
    int age = 0;
    int frozen = 0; // ice lance: ticks left frozen
    int flash = 0;  // hit flash
    int variant = 0;
    int shots = 0;  // a pattern's shot counter
    gamelib::core::Vulnerable vulnerable;
};

struct ShotData {
    Weapon weapon = Weapon::Buster;
    int charge = 0;        // buster: 0 small, 1 half, 2 full
    bool enemy = false;    // fired by an enemy or a boss
    int look = 0;          // enemy shots: 0 pellet, 1 bomb, 2 boss orb, 3 spark, 4 icicle, 5 seed, 6 fire, 7 feather, 8 laser
    bool deflected = false; // bounced off a shield: harmless
    int life = 0;
};
using Shots = gamelib::core::Projectiles<ShotData>;
using Shot = gamelib::core::Projectile<ShotData>;

enum class PickupKind : std::uint8_t { SmallHealth, BigHealth, SmallEnergy, BigEnergy, ExtraLife, Tank };

struct Pickup {
    PickupKind kind = PickupKind::SmallHealth;
    gamelib::core::WalkerBody body;
    gamelib::core::Rect previous;
    int room = 0;
    int life = -1;    // ticks left for drops (-1: placed, stays)
    int placed = -1;  // index of a placed pickup (taken ones stay taken)
};

enum class BossState : std::uint8_t {
    Waiting, // not met yet
    Intro,   // falls in and poses
    Idle,
    Move,
    Jump,
    Attack,
    Attack2,
    Attack3,
    Recover,
    Dead,
};

struct Boss {
    BossKind kind = BossKind::Grinder;
    gamelib::core::WalkerBody body;
    gamelib::core::Rect previous;
    gamelib::core::Vec2 home;   // the marker
    int room = 0;
    int hp = kBossHealth;
    int shown = 0;              // the health bar while it fills
    int pattern = -1;
    int count = 0;              // a pattern's counter
    int flash = 0;
    bool flying = false;
    gamelib::core::Vec2 target; // a dive's or a strike's aim, fixed at the telegraph
    gamelib::core::StateMachine<BossState> sm{BossState::Waiting};
    gamelib::core::PhaseByHealth phases{0.5f, 0.25f};
    gamelib::core::Vulnerable vulnerable;
};

struct Platform {
    gamelib::core::Rail rail;
    gamelib::core::FallingPlatform drop;
    bool falling = false;
    gamelib::core::Rect box;
    gamelib::core::Rect start;
    gamelib::core::Rect previous;
    gamelib::core::Vec2 delta;
    int id = 0;
    int room = 0;
    int ticks = 0;
};

struct Hero {
    gamelib::core::PlatformerBody body;
    gamelib::core::Rect previous;
    gamelib::core::Vulnerable vulnerable;
    gamelib::core::Resource health{kMaxHealth, kMaxHealth};
    Weapon weapon = Weapon::Buster;
    std::array<gamelib::core::Resource, kWeaponCount> energy{};
    std::uint8_t weapons = 0;
    bool sliding = false;
    int slideTicks = 0;
    int shootTicks = 0; // the shooting pose (and the ladder hold)
    int charge = 0;     // ticks the buster has been charging
    float walked = 0.0f;
    int hurtTicks = 0;  // the hurt pose
};

// A room of the stage: a one-screen-tall rectangle the camera stays in.
struct Room {
    gamelib::core::Rect bounds;        // world units
    gamelib::core::CellRange cells;
    gamelib::core::Spawner spawner{0};
    bool windy = false;
    bool miniBossBeaten = false;
    int boss = -1;                     // index into the bosses, or -1
};

struct Checkpoint {
    int room = 0;
    gamelib::core::Vec2 feet;
};

// A boss door: its cells (in the room behind it) and how open it is.
struct Door {
    std::vector<gamelib::core::Cell> cells;
    int room = 0;
};

enum class EventKind {
    Shoot,        // weapon, value: charge
    ChargeReady,  // the full charge is reached
    Jump,
    Land,
    Slide,
    HitEnemy,     // weapon
    Deflect,
    EnemyDown,    // enemy, value: 1 big
    EnemyShot,
    Bomb,         // a lobbed bomb burst
    Hurt,
    Death,
    Pickup,       // pickup
    Checkpoint,
    Scroll,
    DoorOpen,
    DoorClose,
    Unlock,       // a mini-boss's shutter opened
    BossIntro,    // boss
    BossFill,     // one unit of the boss's bar filled
    BossHit,      // value: hp left
    BossPhase,    // value: phase
    BossDown,     // boss
    BossLand,     // a heavy landing: shake
    Strike,       // a lightning bolt, a thorn or an icicle lands
    WeaponSwitch, // weapon
    Tank,         // an energy tank was used
    BeamIn,
    BeamOut,
    Clear,
};

struct Event {
    EventKind kind = EventKind::Shoot;
    gamelib::core::Vec2 at;
    int value = 0;
    Weapon weapon = Weapon::Buster;
    EnemyKind enemy = EnemyKind::Crawler;
    BossKind boss = BossKind::Grinder;
    PickupKind pickup = PickupKind::SmallHealth;
};

class Session {
public:
    Session(StageId stage, const Carry& carry);

    void tick(const Input& input);
    // Pause-menu actions between ticks.
    void selectWeapon(Weapon weapon);
    bool useTank(); // refills health; false without a tank or at full health
    // A developer's start: the hero in room `room` on the floor of `column`.
    void debugStartAt(int room, int column);

    [[nodiscard]] StageId stageId() const { return m_stage; }
    [[nodiscard]] const StageDef& def() const { return stage(m_stage); }
    [[nodiscard]] Phase phase() const { return m_phase; }
    [[nodiscard]] Outcome outcome() const { return m_outcome; }
    [[nodiscard]] int phaseTicks() const { return m_phaseTicks; }
    [[nodiscard]] Carry carry() const;

    [[nodiscard]] const Hero& hero() const { return m_hero; }
    [[nodiscard]] int lives() const { return m_lives; }
    [[nodiscard]] int tanks() const { return m_tanks; }
    [[nodiscard]] bool owns(Weapon weapon) const;
    [[nodiscard]] const gamelib::core::TileMap& map() const { return m_map; }
    [[nodiscard]] gamelib::core::TileWorld tileWorld() const;
    [[nodiscard]] std::span<const Room> rooms() const { return m_rooms; }
    [[nodiscard]] int roomIndex() const { return m_room; }
    [[nodiscard]] const Room& room() const { return m_rooms[static_cast<std::size_t>(m_room)]; }
    // The camera's rectangle in the rules (the hero centred, held in the room).
    [[nodiscard]] gamelib::core::Rect view() const;
    [[nodiscard]] const gamelib::core::SlotMap<Enemy>& enemies() const { return m_enemies; }
    [[nodiscard]] const Shots& shots() const { return m_shots; }
    [[nodiscard]] const std::vector<Pickup>& pickups() const { return m_pickups; }
    [[nodiscard]] const std::vector<Platform>& platforms() const { return m_platforms; }
    [[nodiscard]] std::span<const Boss> bosses() const { return m_bosses; }
    // The boss of the current room while it is met (intro, fight or down).
    [[nodiscard]] const Boss* activeBoss() const;
    [[nodiscard]] std::span<const Checkpoint> checkpoints() const { return m_checkpoints; }
    [[nodiscard]] int checkpointReached() const { return m_checkpoint; }
    [[nodiscard]] std::span<const Door> doors() const { return m_doors; }
    // How open a door is, 0 closed .. 1 open.
    [[nodiscard]] float doorOpen(int door) const;
    // The scroll in progress: 0..1 (also during a gate's scroll).
    [[nodiscard]] float scrollProgress() const;
    // The room a scroll is going to (the current room when none is under way).
    [[nodiscard]] int scrollTarget() const { return m_phase == Phase::Scroll || (m_phase == Phase::Gate && m_gateStep == 2) ? m_scrollTo : m_room; }
    [[nodiscard]] int ticks() const { return m_ticks; }
    // Seconds-free wind push of a windy room (units per tick, 0 when calm).
    [[nodiscard]] float wind() const { return m_wind; }
    // Lightning, thorns and icicles announced before they strike: where and
    // how many ticks are left.
    struct Warning {
        gamelib::core::Vec2 at;
        int ticks = 0;
        int kind = 0; // 0 lightning, 1 thorn, 2 icicle, 3 debris
    };
    [[nodiscard]] std::span<const Warning> warnings() const { return m_warnings; }
    [[nodiscard]] int enemiesDestroyed() const { return m_destroyed; }

    [[nodiscard]] gamelib::core::EventQueue<Event>& events() { return m_events; }
    [[nodiscard]] std::uint64_t hash() const;

private:
    void load();
    void placeHero(gamelib::core::Vec2 feet, int room);
    void enterRoom(int room);
    [[nodiscard]] int roomAt(gamelib::core::Vec2 point) const;
    [[nodiscard]] gamelib::core::Rect viewOf(int room, gamelib::core::Rect heroBox) const;

    // session.cpp
    void playTick(const Input& input);
    void scrollTick();
    void gateTick();
    void checkRooms();
    void checkCheckpoints();
    void stepPickups();
    void stepPlatforms();
    void stepShots();
    void stepWarnings();
    void hurtHero(int damage, const gamelib::core::Rect& source);
    void die();
    void dropPickup(gamelib::core::Vec2 at);
    void startGate(int door);
    void setDoor(const Door& door, bool closed);
    void unlockRoom(int room);
    void warn(gamelib::core::Vec2 at, int ticks, int kind);

    // hero.cpp
    void stepHero(const Input& input);
    void fire(const Input& input);
    void releaseCharge();
    void switchWeapon(int step);
    [[nodiscard]] bool canStand() const;
    void endSlide();

    // enemies.cpp
    void spawnEnemy(int point);
    void stepEnemies();
    void stepEnemy(Enemy& enemy);
    void heroMeetsEnemies();
    void shotsHitEnemies();
    void enemyDown(Enemy& enemy);
    void enemyFire(gamelib::core::Vec2 at, gamelib::core::Vec2 velocity, int look, int damage);

    // bosses.cpp
    void startBossIntro(int boss);
    void stepBosses();
    void stepBoss(Boss& boss);
    void shotsHitBoss(Boss& boss);
    void bossDefeated(Boss& boss);
    void bossFire(const Boss& boss, gamelib::core::Vec2 at, gamelib::core::Vec2 velocity, int look, int damage,
                  float gravity = 0.0f);
    void stepGrinder(Boss& boss);
    void stepStorm(Boss& boss);
    void stepRime(Boss& boss);
    void stepThorn(Boss& boss);
    void stepCinder(Boss& boss);
    void stepGale(Boss& boss);
    void stepOverseer(Boss& boss);

    StageId m_stage = StageId::Intro;
    Carry m_start;
    int m_lives = kStartLives;
    int m_tanks = 0;
    std::uint8_t m_rematches = 0;
    gamelib::core::TileMap m_map{1, 1, kTile};
    std::vector<Room> m_rooms;
    int m_room = 0;
    Hero m_hero;
    gamelib::core::SlotMap<Enemy> m_enemies;
    Shots m_shots{96};
    std::vector<Pickup> m_pickups;
    std::vector<bool> m_placedTaken;
    std::vector<Platform> m_platforms;
    std::vector<Boss> m_bosses;
    std::vector<Checkpoint> m_checkpoints;
    std::vector<Door> m_doors;
    std::vector<Warning> m_warnings;
    gamelib::core::Rng m_rng;
    Phase m_phase = Phase::Ready;
    Outcome m_outcome = Outcome::None;
    int m_phaseTicks = 0;
    int m_ticks = 0;
    int m_checkpoint = -1;
    int m_destroyed = 0;
    float m_wind = 0.0f;
    // A scroll: the rooms and the hero's start and end.
    int m_scrollFrom = 0;
    int m_scrollTo = 0;
    gamelib::core::Vec2 m_scrollHeroFrom;
    gamelib::core::Vec2 m_scrollHeroTo;
    // A gate: the door, its stage (0 opening, 1 walking, 2 scrolling, 3 closing).
    int m_gate = -1;
    int m_gateStep = 0;
    std::vector<int> m_doorOpen; // ticks of opening per door (0 closed .. kGateTicks open)
    gamelib::core::EventQueue<Event> m_events;
};

} // namespace blaster::game
