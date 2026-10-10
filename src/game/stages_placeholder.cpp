// Temporary stages while the engine is built.
#include "stages.hpp"

namespace blaster::game {

namespace {

StageDef _placeholder(const std::string_view id, const std::string_view name, const Theme theme, const BossKind boss,
                      const std::string_view bossRow)
{
    return StageDef{
        .id = id,
        .name = name,
        .theme = theme,
        .boss = boss,
        .rooms = {
            {.attach = Attach::Right, .rows = {
                "########################################",
                "########################################",
                "##                                      ",
                "##                                      ",
                "##                                      ",
                "##                                      ",
                "##                                      ",
                "##                                      ",
                "##                   ===                ",
                "##                                      ",
                "##   S          c                h      ",
                "########################################",
                "########################################",
                "########################################",
                "########################################",
            }},
            {.attach = Attach::Right, .rows = {
                "###########################",
                "###########################",
                "#                         #",
                "#                         #",
                "#                         #",
                "#                         #",
                "#                         #",
                "#   ====           ====   #",
                "#                         #",
                "D                         #",
                bossRow,
                "###########################",
                "###########################",
                "###########################",
                "###########################",
            }},
        },
        .hints = {},
    };
}

} // namespace

StageDef grinderStage() { return _placeholder("grinder", "GEARGRINDER", Theme::Factory, BossKind::Grinder, "D                 1       #"); }
StageDef stormStage() { return _placeholder("storm", "STORMCALLER", Theme::Storm, BossKind::Storm, "D                 2       #"); }
StageDef rimeStage() { return _placeholder("rime", "RIMEFANG", Theme::Ice, BossKind::Rime, "D                 3       #"); }
StageDef thornStage() { return _placeholder("thorn", "THORNWOOD", Theme::Forest, BossKind::Thorn, "D                 4       #"); }
StageDef cinderStage() { return _placeholder("cinder", "CINDERMAW", Theme::Magma, BossKind::Cinder, "D                 5       #"); }
StageDef galeStage() { return _placeholder("gale", "GALEWING", Theme::Sky, BossKind::Gale, "D                 6       #"); }
StageDef fortress1Stage() { return _placeholder("fortress1", "FORTRESS 1", Theme::Fortress, BossKind::Count, "D                 1       #"); }
StageDef fortress2Stage() { return _placeholder("fortress2", "FORTRESS 2", Theme::Fortress, BossKind::Count, "D                 4       #"); }
StageDef fortress3Stage() { return _placeholder("fortress3", "FORTRESS 3", Theme::Fortress, BossKind::Overseer, "D                 7       #"); }

} // namespace blaster::game
