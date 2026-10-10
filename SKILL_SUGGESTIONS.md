# Skill suggestions

Skills that would have made work on this project go better. See `AGENTS.md` for
how this file is maintained.

## Already installed

In `~/.claude/skills/` (checked 2026-10-11):

- action-combat-frame-data
- action-rpg-progression-design
- arcade-action-physics
- articulated-2d-character-rigs
- chiptune-music-sequencing
- chiptune-sfx-synthesis
- ci-cross-platform
- cmake-build-developer
- cmake-library-consumption
- computer-graphics-developer
- cozy-sim-design
- cpp-debugging-profiling
- cpp-performance-developer
- cpp-testing
- cpp26-developer
- deterministic-replay-testing
- game-audio-developer
- game-engine-developer
- game-feel-designer
- game-library-extraction
- game-persistence
- game-ui-text-and-dialogue
- game-visual-design
- glfw-platform-developer
- glsl-shader-build-embedding
- grid-algorithms-for-games
- headless-balance-sweeps
- input-rebinding-ui
- learning-mentor
- logic-puzzle-demo-players
- metroidvania-world-design
- platformer-boss-design
- platformer-controller-feel
- platformer-demo-bots
- platformer-level-design
- pong-arcade-rules
- procedural-dungeon-generation
- procedural-landscape-shaders
- puzzle-game-design-undo
- retro-crt-screen-shader
- sdf-2d-shape-shaders
- shader-developer
- software-architecture-developer
- stroke-font-design
- technical-docs-writer
- text-level-authoring-tools
- tilemap-and-camera-2d
- vector-arcade-neon-look
- vulkan-app-smoke-testing
- vulkan-developer
- vulkan-offscreen-postprocess
- vulkan-text-and-ui
- win32-game-window-automation
- window-screenshot-bursts
- windows-release-packaging

## Suggested

### support-aware-bot-guidance

How to guide a look-ahead platformer bot (plans tried on copies of the
simulation, rated by a distance to the goal) without local minima: a flood
fill over open cells points straight up at an exit above the hero, so the
bot stands under it forever; the fix is a support-aware distance (standable
cells, jump envelopes from the controller's numbers, ladders, lifts and
falling platforms as edges) or level rules that keep the open-cell distance
monotone along the real path (walls under exits, shafts closed above the
platforms). Include recipes for settling rules in trials (stop after
landing, but never on a platform that drops), damage-dealt ratings for boss
fights, firing inside the trials, and auto-grabbing ladders after an
upward room scroll.

Why: Blaster's demo bot stalled in three rooms (a canopy, a lift shaft, the
fortress shaft) because the open-cell distance pointed through a ceiling,
and twice more on falling platforms and lifts because of its settle rule;
each took a probe driver to understand.

### robot-master-stage-design

Mega Man-style stage structure as data: one-screen rooms chained right, up
and down, a theme mechanic introduced safely and then combined (conveyors,
wind, ice, lava, falling bridges, lifts), two checkpoints, a guardian room
with a shutter, a boss door corridor with a refill, enemy placement per
screen (one new threat per room), weapon weaknesses as a ring, and fortress
stages with rematches behind doors. With numbers for a 3.5-tile jump: gap
and ledge limits, platform spacing over spikes, and how wind or ice change
them.

Why: Blaster's ten stages were authored from scratch; several rooms needed
fixes for unreachable ledges (four tiles up), gaps too wide against the
wind and pits that fell into the room below.
