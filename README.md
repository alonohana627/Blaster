# Blaster

A game built on [GameLib](../GameLib) - generated from its new-game template.

Steer the glowing orb with WASD or the arrow keys and bump the edges as
often as you can in 30 seconds; Space changes its color, Escape or P pauses,
F11 toggles fullscreen. Everything on screen is drawn by shaders; the sounds
are synthesized. Settings and the best score are kept in
`blaster.ini` in the user data folder (`%APPDATA%\Blaster` on
Windows; the environment variable `BLASTER_DATA_DIR` moves it).

## Build and run

Requirements are GameLib's (C++26 Clang, CMake 3.28+, Ninja, Vulkan 1.3,
glslc). GameLib is found at `../GameLib` relative to this folder; to use
another checkout add `-DFETCHCONTENT_SOURCE_DIR_GAMELIB=<path>` to the
configure command.

```powershell
cmake --preset debug
cmake --build --preset debug
build/debug/bin/blaster.exe
ctest --preset debug    # smoke test: 120 frames with the validation layer
```

## Where to start

- `src/main.cpp`: the game - `tick()` simulates at a fixed 120 Hz, `frame()`
  runs once per frame for effects, `draw()` fills the draw list. The title,
  pause and game-over menus are GameLib's `app::ArcadeMenus`, the settings
  file its `app::ArcadeProfile` (settings and named bests; keys of the game's
  own through `openArcadeProfile<Keys>`). A game with more to keep than these settings
  and bests (a save game) keeps its own `storage::Persisted<T>` (with
  `app::StandardSettings` inside) and still uses `ArcadeMenus`: its own
  settings go on the title menu after PLAY (the constructor's third
  argument), and `setGameOverTitle` names a round that was won.
- `shaders/orb.frag`: a custom shape; add more with `gamelib_add_shaders()` in
  `CMakeLists.txt` and `renderer().addShape()`.
- GameLib's docs: `GETTING_STARTED.md`, `RENDERING.md`, `EXTENDING_GAMELIB.md`.
