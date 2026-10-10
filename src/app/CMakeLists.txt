# blaster_app: everything around the rules - the GameLib main-loop glue, the
# profile file and key bindings, the stage view and its shaders, sounds,
# music and effects. Menus, settings, the controls screen, sound banks,
# transitions, effects and the attract mode come from GameLib (app, ui,
# audio, fx, ai).
add_library(blaster_app STATIC
    actions.cpp
    blaster_game.cpp
    effects.cpp
    palette.cpp
    profile.cpp
    sounds.cpp
    view.cpp)
target_include_directories(blaster_app PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(blaster_app PUBLIC blaster_game GameLib::app GameLib::ui GameLib::fx GameLib::ai)
gamelib_target_warnings(blaster_app)
# Blaster's own procedural shapes, embedded as SPIR-V (see view.hpp, Shapes).
gamelib_add_shaders(blaster_app SHADERS
    ${PROJECT_SOURCE_DIR}/shaders/backdrop.frag
    ${PROJECT_SOURCE_DIR}/shaders/boss.frag
    ${PROJECT_SOURCE_DIR}/shaders/enemy.frag
    ${PROJECT_SOURCE_DIR}/shaders/hero.frag
    ${PROJECT_SOURCE_DIR}/shaders/item.frag
    ${PROJECT_SOURCE_DIR}/shaders/shot.frag
    ${PROJECT_SOURCE_DIR}/shaders/tile.frag)
