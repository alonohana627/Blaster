#include "stages.hpp"

#include "rules.hpp"

#include <array>
#include <stdexcept>
#include <string>

namespace blaster::game {

namespace {

// helpers

const std::array<StageDef, kStageCount>& _stages()
{
    static const std::array<StageDef, kStageCount> stages{
        introStage(),  grinderStage(), stormStage(),     rimeStage(),      thornStage(),
        cinderStage(), galeStage(),    fortress1Stage(), fortress2Stage(), fortress3Stage(),
    };
    return stages;
}

[[noreturn]] void _fail(const StageDef& def, const std::size_t room, const std::string& what)
{
    throw std::invalid_argument("stage " + std::string{def.id} + " room " + std::to_string(room) + ": " + what);
}

} // namespace

const StageDef& stage(const StageId id)
{
    return _stages()[static_cast<std::size_t>(id)];
}

StageId stageOf(const BossKind boss)
{
    return static_cast<StageId>(static_cast<int>(boss) + 1);
}

StageLayout layoutStage(const StageDef& def)
{
    std::vector<gamelib::core::RoomText> rooms;
    for (std::size_t i = 0; i < def.rooms.size(); ++i) {
        const RoomDef& room = def.rooms[i];
        // The camera is locked to one screen of rows: every room is exactly that tall.
        if (room.rows.size() != static_cast<std::size_t>(kRoomRows)) {
            _fail(def, i, "has " + std::to_string(room.rows.size()) + " rows, expected " + std::to_string(kRoomRows));
        }
        rooms.push_back({.rows = room.rows, .placement = {.attach = room.attach, .along = room.column}});
    }
    try {
        gamelib::core::RoomChain chain = gamelib::core::layoutRooms(rooms, {kMinRoomColumns, kRoomRows});
        return StageLayout{.rows = std::move(chain.rows), .rooms = std::move(chain.rooms)};
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument("stage " + std::string{def.id} + ": " + error.what());
    }
}
} // namespace blaster::game
