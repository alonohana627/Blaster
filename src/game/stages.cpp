#include "stages.hpp"

#include "rules.hpp"

#include <algorithm>
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
    using gamelib::core::Cell;
    using gamelib::core::CellRange;
    if (def.rooms.empty()) {
        throw std::invalid_argument("stage " + std::string{def.id} + " has no rooms");
    }
    // Each room's top-left cell, from the attachments.
    std::vector<CellRange> rooms;
    for (std::size_t i = 0; i < def.rooms.size(); ++i) {
        const RoomDef& room = def.rooms[i];
        if (room.rows.size() != static_cast<std::size_t>(kRoomRows)) {
            _fail(def, i, "has " + std::to_string(room.rows.size()) + " rows, expected " + std::to_string(kRoomRows));
        }
        const int width = static_cast<int>(room.rows.front().size());
        for (std::size_t r = 0; r < room.rows.size(); ++r) {
            if (static_cast<int>(room.rows[r].size()) != width) {
                _fail(def, i, "row " + std::to_string(r) + " is " + std::to_string(room.rows[r].size())
                                  + " wide, expected " + std::to_string(width));
            }
        }
        if (width < kMinRoomColumns) {
            _fail(def, i, "is " + std::to_string(width) + " columns wide, at least " + std::to_string(kMinRoomColumns)
                              + " needed");
        }
        Cell origin{0, 0};
        if (i > 0) {
            const CellRange& prev = rooms.back();
            const int prevWidth = prev.max.x - prev.min.x + 1;
            switch (room.attach) {
            case Attach::Right: origin = {prev.max.x + 1, prev.min.y}; break;
            case Attach::Up:
            case Attach::Down: {
                const int x = room.column == kAlignRight ? prev.min.x + prevWidth - width : prev.min.x + room.column;
                origin = {x, room.attach == Attach::Up ? prev.min.y - kRoomRows : prev.max.y + 1};
                break;
            }
            }
        }
        const CellRange range{.min = origin, .max = {origin.x + width - 1, origin.y + kRoomRows - 1}};
        for (std::size_t j = 0; j < rooms.size(); ++j) {
            const CellRange& other = rooms[j];
            if (range.min.x <= other.max.x && range.max.x >= other.min.x && range.min.y <= other.max.y
                && range.max.y >= other.min.y) {
                _fail(def, i, "overlaps room " + std::to_string(j));
            }
        }
        rooms.push_back(range);
    }
    // Shift everything to start at cell (0, 0).
    Cell low = rooms.front().min;
    Cell high = rooms.front().max;
    for (const CellRange& r : rooms) {
        low = {std::min(low.x, r.min.x), std::min(low.y, r.min.y)};
        high = {std::max(high.x, r.max.x), std::max(high.y, r.max.y)};
    }
    StageLayout layout;
    const int width = high.x - low.x + 1;
    const int height = high.y - low.y + 1;
    layout.rows.assign(static_cast<std::size_t>(height), std::string(static_cast<std::size_t>(width), ' '));
    for (std::size_t i = 0; i < rooms.size(); ++i) {
        CellRange& r = rooms[i];
        r.min = {r.min.x - low.x, r.min.y - low.y};
        r.max = {r.max.x - low.x, r.max.y - low.y};
        for (int y = 0; y < kRoomRows; ++y) {
            const std::string_view row = def.rooms[i].rows[static_cast<std::size_t>(y)];
            layout.rows[static_cast<std::size_t>(r.min.y + y)].replace(static_cast<std::size_t>(r.min.x), row.size(), row);
        }
    }
    layout.rooms = std::move(rooms);
    return layout;
}

} // namespace blaster::game
