# Blaster

An action platformer campaign in the spirit of the 8-bit robot shooters,
built on [GameLib](https://github.com/alonohana627/GameLib). You are Rivet,
a small robot with an arm cannon: clear the lab's outskirts, then take on
six robot masters in any order - Geargrinder, Stormcaller, Rimefang,
Thornwood, Cindermaw and Galewing - win each one's weapon (every master is
weak to another's), and storm the Overseer's three-stage fortress.

Everything on screen is drawn by shaders (no image files) and every sound
and tune is synthesized; the simulation runs on fixed 60 Hz ticks.

## Controls

Every key can be rebound on the CONTROLS screen (title or pause menu);
gamepads work too.

| Action | Keys | Gamepad |
|---|---|---|
| Move | Left / Right (A / D) | D-pad, left stick |
| Climb a ladder | Up / Down (W / S) | D-pad, left stick |
| Jump (hold for height) | Z, Space, K | A |
| Shoot (hold to charge the buster) | X, J | X |
| Slide (or Down + Jump) | C, L | B |
| Previous / next weapon | Q / E | LB / RB |
| Pause (WEAPONS menu, tanks) | Escape, P | Start |
| Fullscreen | F11 | |

## Play

- The intro stage teaches running, jumping, shooting, sliding, ladders and
  the charge shot, and ends with a guardian.
- The stage select shows the six robot masters; beaten ones stay beaten
  and their weapons are yours. When all six are down the fortress opens.
- Each stage has two checkpoints, a guardian behind a shutter and a boss
  door. Bosses change their pattern below half and a quarter of their
  health.
- Energy tanks refill your health from the pause menu's WEAPONS screen;
  you keep the ones you carry between sessions.
- Progress, tanks, settings and key bindings are kept in `blaster.ini` in
  the user data folder (`%APPDATA%\Blaster` on Windows; `BLASTER_DATA_DIR`
  moves it).

## Build and run

Requirements are GameLib's (C++26 Clang, CMake 3.28+, Ninja, Vulkan 1.3,
glslc). CMake fetches GameLib from GitHub at the commit pinned in
`CMakeLists.txt`.

```powershell
cmake --preset debug
cmake --build --preset debug
build/debug/bin/blaster.exe          # --demo: watch the demo bot play the intro
ctest --preset debug                 # smoke test: 120 frames with the validation layer
```
