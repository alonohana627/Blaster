# blaster_game: the rules - the stages and their rooms, the hero (GameLib's
# platformer controller with a slide, the buster and the special weapons),
# enemies, bosses, pickups, the demo bot. Depends only on GameLib::core and
# GameLib::ai (the demo bot's planner): no window, GPU, audio or clock,
# deterministic for the per-tick input.
add_library(blaster_game STATIC
    bosses.cpp
    demo_bot.cpp
    enemies.cpp
    hero.cpp
    rules.cpp
    session.cpp
    stages.cpp
    stages_intro.cpp
    stages_placeholder.cpp
    weapons.cpp)
target_include_directories(blaster_game PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(blaster_game PUBLIC GameLib::core GameLib::ai)
gamelib_target_warnings(blaster_game)
