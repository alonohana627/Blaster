# Features

What Blaster does today, verified by running it (on screen, or with the
demo bot in a headless driver). Updated with each feature.

## Campaign

- An intro stage that teaches moving, jumping, shooting, sliding, ladders,
  the charge shot and spikes (hints written on the backdrop with the keys
  the player bound), ending with a guardian.
- Six robot master stages, each with its own theme and mechanic
  (Geargrinder's conveyor belts, Stormcaller's gusts, Rimefang's ice,
  Thornwood's vines and thorns, Cindermaw's lava, Galewing's falling
  bridges and lifts), 10-11 one-screen rooms joined by locked camera
  scrolls (runs, shafts going up and down, a boss door), two checkpoints,
  a guardian behind a shutter and the boss.
- Three fortress stages: two end in rematches with three robot masters
  each behind boss doors (beaten ones stay beaten after a death), the third
  climbs to the Overseer.
- Every stage, bosses included, was played to its end by the demo bot.

## The hero

- GameLib's platformer controller: instant run, a 3.5-tile jump cut on
  release, ladders (turning and shooting while hanging), the slide under
  one-tile gaps (the slide button or Down + Jump).
- The buster: three shots on screen; holding fire charges a bigger shot,
  then a piercing full charge (GameLib's ChargeMeter).
- Six special weapons won from the bosses, each with an energy bar: Saw
  Blade (pierces), Spark Shot (zigzags), Ice Lance (freezes enemies), Leaf
  Spread (three ways), Magma Lob (bounces), Gale Wing (homes in). Switched
  with two keys or the pause menu's WEAPONS screen; the hero recolors.
- 28 units of health, lives, energy tanks; knockback away from the source
  and a second of invincibility with a flicker after a hit; spikes kill
  unless flickering, lava and pits always.

## Enemies and bosses

- Twelve enemy kinds: crawler, hopper, diver, shield walker (deflects shots
  until it opens to fire), turret (shielded until it opens), nest (releases
  drones), drone, roller, lobber (arcing bombs), popper (hides, pops up with
  a three-way spread), wisp (sine flight), dropper (drops from the ceiling).
  They come back from their spawn points when scrolled off screen and back.
- A guardian per stage (hop, spread and charge, angrier at half health)
  that keeps its room's exit shut until it falls.
- Six robot masters and the Overseer, each with an intro (falls in, the
  health bar fills), two or three patterns chosen without repeats, a new
  set below half and below a quarter of their health, contact and
  projectile damage, telegraphed strikes (lightning, thorns, icicles,
  debris, a laser to jump or slide under) and a weakness to one weapon.

## Look and sound

- Every visual is a shader: themed parallax backdrops (sky, far and near
  silhouettes, weather in front), tiles per theme, the hero, the enemies
  and the bosses as layered SDF bodies, shots, pickups, HUD segment bars.
- Muzzle flashes, impact sparks, explosion rings and debris, the death
  burst of orbs, screen shake and flash, hit stop on boss hits.
- Synthesized sound effects (shots, the charge loop, hits, explosions,
  ladder steps, the boss door, the weapon get) and fourteen tunes: title,
  stage select, a theme per stage, the boss theme, the Overseer's theme,
  the weapon get and the ending.

## Screens and saves

- Title with the demo bot playing the intro behind the menu; `--demo` plays
  it full screen.
- Stage select, stage cards and READY, pause (resume, settings, restart,
  weapons, exit stage), a controls screen to rebind every key and button,
  the weapon-get scene, game over and the ending with the campaign's
  statistics.
- `blaster.ini`: settings, key bindings, the robot masters beaten, the
  fortress progress, energy tanks and the statistics.
