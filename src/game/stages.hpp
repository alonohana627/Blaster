#pragma once

// The ten stages as text: the intro, the six robot masters' stages and the
// three fortress stages. A stage is a chain of one-screen-tall rooms, each
// written as its own block of 15 rows and attached to the room before it
// (to its right, above or below), so a horizontal run, a vertical shaft of
// stacked screens and the boss room read as separate blocks. The camera is
// locked to one room at a time and scrolls when the hero crosses into the
// next one.
//
// Tiles (the character is the tile id):
//   ' ' empty        '#' ground and walls    'B' blocks (a second look)
//   '=' one-way      'H' ladder              '^' spikes (deadly)
//   '~' lava, acid or a bottomless drop (deadly)
//   'I' ice (slippery)  '<' '>' conveyor belts  'D' boss door
//   'L' a shutter that opens when the room's mini-boss is beaten
//   '-' '|' rails of a lift ('M' marker)
// Markers (empty space behind them):
//   'S' start   'K' checkpoint   'W' a windy room   'M' lift on a rail
//   'F' falling platform
//   enemies: 'c' crawler 'h' hopper 'd' diver 'w' shield walker 't' turret
//            'n' nest 'g' drone 'r' roller 'l' lobber 'p' popper
//            'f' wisp 'b' dropper 'm' the stage's mini-boss
//   bosses: '1'..'6' the robot masters in select order, '7' the Overseer
//   items: 'E' energy tank, 'U' extra life, '$' big health, '&' big weapon energy

#include "weapons.hpp"

#include <gamelib/core/math.hpp>
#include <gamelib/core/tile_map.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace blaster::game {

enum class Theme : std::uint8_t { Lab, Factory, Storm, Ice, Forest, Magma, Sky, Fortress, Count };
inline constexpr int kThemeCount = static_cast<int>(Theme::Count);

// Where a room goes, relative to the room before it.
enum class Attach : std::uint8_t {
    Right, // the same screen row, its left edge at the previous room's right edge
    Up,    // one screen up
    Down,  // one screen down
};

// Up and Down rooms line up their right edges with the previous room's
// unless `column` says where their left edge goes (in the previous room's
// columns).
inline constexpr int kAlignRight = -1000;

struct RoomDef {
    Attach attach = Attach::Right;
    int column = kAlignRight;
    std::vector<std::string_view> rows; // 15 rows of one width, at least 27 columns
};

// A line of teaching text written on the backdrop of a room, at a cell of
// that room. "{jump}" and the other action names in braces are replaced by
// the keys the player bound.
struct Hint {
    int room = 0;
    gamelib::core::Vec2 cell; // room cells, the text's center
    std::string_view text;
};

struct StageDef {
    std::string_view id;   // "factory"
    std::string_view name; // "GEARGRINDER'S FOUNDRY"
    Theme theme = Theme::Lab;
    BossKind boss = BossKind::Count; // the robot master (Count: none)
    int miniBoss = 0;                // the mini-boss's variant
    std::vector<RoomDef> rooms;
    std::vector<Hint> hints;
};

enum class StageId : std::uint8_t {
    Intro,
    Grinder,
    Storm,
    Rime,
    Thorn,
    Cinder,
    Gale,
    Fortress1,
    Fortress2,
    Fortress3,
    Count
};
inline constexpr int kStageCount = static_cast<int>(StageId::Count);
inline constexpr int kFortressStages = 3;

[[nodiscard]] const StageDef& stage(StageId id);
// The robot master's stage.
[[nodiscard]] StageId stageOf(BossKind boss);
[[nodiscard]] constexpr StageId fortressStage(const int index)
{
    return static_cast<StageId>(static_cast<int>(StageId::Fortress1) + index);
}

inline constexpr std::string_view kMarkerSymbols = "SKWMFchdwtngrlpfbm1234567EU$&";

// A stage's rooms put together into one map: the rows (cells outside every
// room are empty, so a pit's bottom is open) and each room's cells. Throws std::invalid_argument
// naming the stage, the room and the row of a room with the wrong height,
// a row of the wrong width, a room narrower than a screen or rooms that
// overlap.
struct StageLayout {
    std::vector<std::string> rows;
    std::vector<gamelib::core::CellRange> rooms;
};
[[nodiscard]] StageLayout layoutStage(const StageDef& def);

// The stage files (stages_*.cpp).
[[nodiscard]] StageDef introStage();
[[nodiscard]] StageDef grinderStage();
[[nodiscard]] StageDef stormStage();
[[nodiscard]] StageDef rimeStage();
[[nodiscard]] StageDef thornStage();
[[nodiscard]] StageDef cinderStage();
[[nodiscard]] StageDef galeStage();
[[nodiscard]] StageDef fortress1Stage();
[[nodiscard]] StageDef fortress2Stage();
[[nodiscard]] StageDef fortress3Stage();

} // namespace blaster::game
