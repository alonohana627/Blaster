# GameLib revision

Blaster and GameLib are separate repositories, and commits across
repositories are not atomic. This file records which GameLib revision each
Blaster change was validated against (build with zero warnings, smoke run
with the validation layer). `CMakeLists.txt` pins GameLib by full commit hash
(`GIT_TAG`, `GIT_SHALLOW FALSE`), so checking out a Blaster commit reproduces
its combination by itself.

| Blaster change | GameLib revision | Validated |
|---|---|---|
| Project from the new-game template (`build`) | `dfb75a3` (0.0.8 plus docs) | clean debug build fetching the commit, zero warnings, 1/1 smoke run |
| The rules, the intro stage, the app and its shaders (`feat`) | `dfb75a3` | debug build with zero warnings; 1/1 smoke run; the intro played by the demo bot to its guardian in a headless driver; title and the intro checked on screen |
| Re-pin: the slide, room chains (`layoutRooms`), `RoomScroll` and `ChargeMeter` from GameLib replace the game's own (`build`) | `39e4071` | debug build with zero warnings; 1/1 smoke run; the demo bot clears the intro as before |
