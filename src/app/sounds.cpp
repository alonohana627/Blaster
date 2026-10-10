#include "sounds.hpp"

#include <array>
#include <string>
#include <vector>

namespace blaster::app {

namespace {

// helpers

using gamelib::audio::note;
using gamelib::audio::noise;
using gamelib::audio::Note;
using gamelib::audio::Song;
using gamelib::audio::SoundRecipe;
using gamelib::audio::tone;
using gamelib::audio::Track;
using gamelib::audio::Wave;

// A chord for the generated parts: its root as a MIDI number in octave 3
// (C3 = 48) and its quality.
enum class Quality : std::uint8_t { Major, Minor, Dim };
struct Chord {
    int root = 48;
    Quality quality = Quality::Major;
};

std::string _name(const int midi)
{
    static constexpr std::array<const char*, 12> kNames{"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    return std::string{kNames[static_cast<std::size_t>(midi % 12)]} + std::to_string(midi / 12 - 1);
}

int _third(const Chord c)
{
    return c.root + (c.quality == Quality::Major ? 4 : 3);
}

int _fifth(const Chord c)
{
    return c.root + (c.quality == Quality::Dim ? 6 : 7);
}

// Sixteenth-note arpeggios, an octave or two over the root: root, third, fifth, third.
std::string _arp(const std::vector<Chord>& chords, const int octaves = 2)
{
    std::string out;
    for (const Chord c : chords) {
        const std::array<int, 4> tones{c.root, _third(c), _fifth(c), _third(c)};
        for (int i = 0; i < 16; ++i) {
            out += _name(tones[static_cast<std::size_t>(i % 4)] + 12 * octaves) + " ";
        }
        out += "| ";
    }
    return out;
}

// An eighth-note bass jumping between the root and its octave, the fifth at the end of the bar.
std::string _pump(const std::vector<Chord>& chords)
{
    std::string out;
    for (const Chord c : chords) {
        const int low = c.root - 12;
        const std::array<int, 8> steps{low, c.root, low, c.root, low, c.root, _fifth(c) - 12, c.root};
        for (const int s : steps) {
            out += _name(s) + " - ";
        }
        out += "| ";
    }
    return out;
}

// A held bass: one note per half bar.
std::string _held(const std::vector<Chord>& chords)
{
    std::string out;
    for (const Chord c : chords) {
        out += _name(c.root - 12) + " - - - - - - - " + _name(_fifth(c) - 12) + " - - - - - - - | ";
    }
    return out;
}

// One bar of drums on the noise channel: low notes are kicks, high notes hats.
constexpr const char* kDrive = "C3 . C8 C8 C6 . C8 C3 C3 . C8 C8 C6 . C8 C8";
constexpr const char* kLight = "C3 . C8 . C6 . C8 . C3 . C8 C3 C6 . C8 .";
constexpr const char* kSparse = "C3 . . . . . C8 . C6 . . . . . C8 .";

Song _song(const float bpm, const char* lead, const std::vector<Chord>& chords, const char* drums,
           const bool pump = true, const Wave leadWave = Wave::Pulse25, const float arpVolume = 0.28f)
{
    Song s{.bpm = bpm, .tracks = {}, .peakDb = -16.0f};
    s.tracks.push_back(Track{.wave = leadWave, .volume = 0.85f, .loop = lead});
    s.tracks.push_back(Track{.wave = Wave::Pulse12, .volume = arpVolume, .loop = _arp(chords)});
    s.tracks.push_back(Track{.wave = Wave::Triangle, .volume = 1.0f, .loop = pump ? _pump(chords) : _held(chords)});
    if (drums != nullptr) {
        s.tracks.push_back(Track{.wave = Wave::Noise, .volume = 0.3f, .loop = drums});
    }
    return s;
}

constexpr Chord C{48}, D{50}, E{52}, F{53}, G{55}, A{45}, Bb{46}, B{47}, Eb{51}, Ab{44};
constexpr Chord Am{45, Quality::Minor}, Bm{47, Quality::Minor}, Cm{48, Quality::Minor}, Dm{50, Quality::Minor};
constexpr Chord Em{52, Quality::Minor}, Csd{49, Quality::Dim};

} // namespace

SoundRecipe sfxRecipe(const Sfx sfx)
{
    switch (sfx) {
    case Sfx::Shoot: return {{{tone(Wave::Pulse25, note("A5"), 0.06f, note("E5"))}}, -19.0f};
    case Sfx::ShootHalf: return {{{tone(Wave::Pulse50, note("C6"), 0.1f, note("C5"))}, {noise(6000.0f, 0.06f, 2000.0f, 0.4f)}}, -16.0f};
    case Sfx::ShootFull:
        return {{{tone(Wave::Pulse25, note("E6"), 0.2f, note("C4"))}, {noise(7000.0f, 0.18f, 800.0f, 0.7f)}}, -13.0f};
    case Sfx::Charge: {
        // A held warble, looped while the buster charges (its pitch rises with the charge).
        Note a = tone(Wave::Pulse12, note("C6"), 0.06f, note("E6"), 0.8f);
        Note b = tone(Wave::Pulse12, note("E6"), 0.06f, note("C6"), 0.8f);
        a.decay = 0.0f;
        b.decay = 0.0f;
        return {{{a, b}}, -24.0f};
    }
    case Sfx::ChargeReady: return {{{tone(Wave::Pulse25, note("C7"), 0.04f), tone(Wave::Pulse25, note("G7"), 0.08f)}}, -21.0f};
    case Sfx::Weapon:
        return {{{tone(Wave::Pulse12, note("G6"), 0.09f, note("G5"))}, {noise(9000.0f, 0.08f, 3000.0f, 0.5f)}}, -17.0f};
    case Sfx::Land: return {{{tone(Wave::Triangle, note("C3"), 0.05f, note("C2"))}}, -22.0f};
    case Sfx::Slide: return {{{noise(7000.0f, 0.14f, 1500.0f, 0.7f)}}, -22.0f};
    case Sfx::EnemyHit: return {{{tone(Wave::Pulse12, note("C6"), 0.035f, note("G5"))}, {noise(4000.0f, 0.04f, 1500.0f)}}, -19.0f};
    case Sfx::Deflect: return {{{tone(Wave::Pulse12, note("E7"), 0.07f, note("B6"))}}, -21.0f};
    case Sfx::Explosion:
        return {{{noise(2400.0f, 0.38f, 160.0f)}, {tone(Wave::Triangle, note("G3"), 0.25f, note("C2"))}}, -13.0f};
    case Sfx::Hurt:
        return {{{tone(Wave::Pulse25, note("G5"), 0.04f), tone(Wave::Pulse25, note("E5"), 0.04f),
                  tone(Wave::Pulse25, note("C5"), 0.04f), tone(Wave::Pulse25, note("G4"), 0.08f)},
                 {noise(3000.0f, 0.12f, 600.0f, 0.6f)}},
                -14.0f};
    case Sfx::Death:
        return {{{tone(Wave::Pulse25, note("C6"), 0.08f, note("C5")), tone(Wave::Pulse25, note("A5"), 0.08f, note("A4")),
                  tone(Wave::Pulse25, note("F5"), 0.08f, note("F4")), tone(Wave::Pulse25, note("D5"), 0.08f, note("D4")),
                  tone(Wave::Pulse25, note("C5"), 0.3f, note("C3"))},
                 {noise(1800.0f, 0.62f, 90.0f, 0.8f)}},
                -11.0f};
    case Sfx::Pickup: return {{{tone(Wave::Pulse25, note("C6"), 0.04f), tone(Wave::Pulse25, note("G6"), 0.07f)}}, -18.0f};
    case Sfx::HealthTick: return {{{tone(Wave::Pulse12, note("E6"), 0.03f)}}, -23.0f};
    case Sfx::ExtraLife:
        return {{{tone(Wave::Pulse25, note("E5"), 0.07f), tone(Wave::Pulse25, note("G5"), 0.07f),
                  tone(Wave::Pulse25, note("E6"), 0.07f), tone(Wave::Pulse25, note("C6"), 0.07f),
                  tone(Wave::Pulse25, note("D6"), 0.07f), tone(Wave::Pulse25, note("G6"), 0.16f)}},
                -14.0f};
    case Sfx::Tank:
        return {{{tone(Wave::Pulse25, note("C5"), 0.05f), tone(Wave::Pulse25, note("E5"), 0.05f),
                  tone(Wave::Pulse25, note("G5"), 0.05f), tone(Wave::Pulse25, note("C6"), 0.05f),
                  tone(Wave::Pulse25, note("E6"), 0.05f), tone(Wave::Pulse25, note("G6"), 0.12f)}},
                -14.0f};
    case Sfx::Checkpoint:
        return {{{tone(Wave::Pulse25, note("G5"), 0.07f), tone(Wave::Pulse25, note("D6"), 0.07f),
                  tone(Wave::Pulse25, note("G6"), 0.2f)},
                 {tone(Wave::Triangle, note("G3"), 0.34f)}},
                -15.0f};
    case Sfx::DoorOpen:
        return {{{noise(900.0f, 0.07f, 600.0f), noise(900.0f, 0.07f, 600.0f, 0.0f), noise(1000.0f, 0.07f, 600.0f),
                  noise(900.0f, 0.07f, 600.0f, 0.0f), noise(1100.0f, 0.07f, 600.0f), noise(900.0f, 0.07f, 600.0f, 0.0f),
                  noise(1200.0f, 0.07f, 600.0f)},
                 {tone(Wave::Triangle, note("C2"), 0.49f, note("G2"))}},
                -15.0f};
    case Sfx::BossFill: return {{{tone(Wave::Pulse12, note("A6"), 0.025f)}}, -24.0f};
    case Sfx::BossHit: return {{{tone(Wave::Pulse50, note("G3"), 0.14f, note("C3"))}, {noise(3500.0f, 0.12f, 400.0f)}}, -12.0f};
    case Sfx::BossDown:
        return {{{noise(3000.0f, 1.2f, 80.0f)},
                 {tone(Wave::Pulse25, note("C6"), 0.15f, note("C5")), tone(Wave::Pulse25, note("G5"), 0.15f, note("G4")),
                  tone(Wave::Pulse25, note("E5"), 0.15f, note("E4")), tone(Wave::Pulse25, note("C5"), 0.6f, note("C3"))}},
                -10.0f};
    case Sfx::BossLand: return {{{noise(800.0f, 0.22f, 120.0f)}, {tone(Wave::Triangle, note("C3"), 0.2f, note("C2"))}}, -13.0f};
    case Sfx::Strike:
        return {{{noise(9000.0f, 0.28f, 400.0f)}, {tone(Wave::Pulse12, note("C7"), 0.2f, note("C4"), 0.6f)}}, -12.0f};
    case Sfx::EnemyShot: return {{{tone(Wave::Pulse12, note("D6"), 0.045f, note("A5"))}}, -23.0f};
    case Sfx::Bomb: return {{{noise(1600.0f, 0.26f, 120.0f)}, {tone(Wave::Triangle, note("E3"), 0.18f, note("C2"))}}, -14.0f};
    case Sfx::WeaponSwitch: return {{{tone(Wave::Pulse25, note("G5"), 0.04f), tone(Wave::Pulse25, note("D6"), 0.06f)}}, -18.0f};
    case Sfx::WeaponGet:
        return {{{tone(Wave::Pulse25, note("G5"), 0.1f), tone(Wave::Pulse25, note("B5"), 0.1f), tone(Wave::Pulse25, note("D6"), 0.1f),
                  tone(Wave::Pulse25, note("G6"), 0.3f), tone(Wave::Pulse25, note("F#6"), 0.1f), tone(Wave::Pulse25, note("G6"), 0.5f)},
                 {tone(Wave::Pulse12, note("B4"), 0.3f), tone(Wave::Pulse12, note("D5"), 0.3f), tone(Wave::Pulse12, note("G5"), 0.6f)},
                 {tone(Wave::Triangle, note("G2"), 0.3f), tone(Wave::Triangle, note("D3"), 0.3f), tone(Wave::Triangle, note("G2"), 0.6f)}},
                -11.0f};
    case Sfx::Ladder: return {{{tone(Wave::Triangle, note("G4"), 0.03f)}}, -25.0f};
    case Sfx::Beam: return {{{tone(Wave::Pulse12, note("C5"), 0.12f, note("C7")), tone(Wave::Pulse12, note("C7"), 0.12f, note("G6"))}}, -17.0f};
    case Sfx::Ready:
        return {{{tone(Wave::Pulse25, note("C6"), 0.08f), tone(Wave::Pulse25, note("C6"), 0.05f, 0.0f, 0.0f),
                  tone(Wave::Pulse25, note("G6"), 0.16f)}},
                -17.0f};
    case Sfx::MenuMove: return {{{tone(Wave::Pulse12, note("C7"), 0.03f)}}, -21.0f};
    case Sfx::MenuSelect: return {{{tone(Wave::Pulse25, note("C5"), 0.04f), tone(Wave::Pulse25, note("G5"), 0.07f)}}, -16.0f};
    case Sfx::MenuBack: return {{{tone(Wave::Pulse25, note("G5"), 0.04f), tone(Wave::Pulse25, note("C5"), 0.07f)}}, -16.0f};
    case Sfx::GameOver:
        return {{{tone(Wave::Pulse25, note("E5"), 0.22f), tone(Wave::Pulse25, note("D5"), 0.22f), tone(Wave::Pulse25, note("C5"), 0.22f),
                  tone(Wave::Pulse25, note("B4"), 0.22f), tone(Wave::Pulse25, note("A4"), 0.7f)},
                 {tone(Wave::Triangle, note("A2"), 0.44f), tone(Wave::Triangle, note("E2"), 0.44f), tone(Wave::Triangle, note("A2"), 0.7f)}},
                -12.0f};
    case Sfx::Unlock:
        return {{{noise(1400.0f, 0.06f, 900.0f), noise(1400.0f, 0.06f, 900.0f, 0.0f), noise(1600.0f, 0.1f, 600.0f)},
                 {tone(Wave::Pulse25, note("C5"), 0.08f), tone(Wave::Pulse25, note("G5"), 0.14f)}},
                -15.0f};
    case Sfx::Count: break;
    }
    return {};
}

Song tune(const Tune t)
{
    switch (t) {
    case Tune::Title: // C major, hopeful and heroic.
        return _song(132.0f,
                     "C5 - E5 - G5 - - - C6 - - - B5 - G5 - | A5 - - - G5 - D5 - - - B4 - D5 - G5 - | "
                     "E5 - - - A5 - - - C6 - B5 - A5 - E5 - | F5 - - - E5 - D5 - C5 - - - G4 - - - | "
                     "C5 - E5 - G5 - - - C6 - - - B5 - G5 - | D6 - - - B5 - G5 - - - A5 - B5 - D6 - | "
                     "C6 - - - B5 - A5 - G5 - E5 - A5 - - - | F5 - A5 - C6 - - - B5 - - - G5 - - -",
                     {C, G, Am, F}, kLight);
    case Tune::Select: // G major, a short jaunty loop.
        return _song(140.0f,
                     "G5 - B5 - D6 - B5 - C6 - - - A5 - - - | E6 - - - D6 - C6 - B5 - A5 - G5 - - - | "
                     "F#5 - A5 - D6 - - - C6 - B5 - A5 - - - | G5 - - - D5 - - - G5 - - - . . . .",
                     {G, C, D, G}, kLight);
    case Tune::Lab: // A minor, driving.
        return _song(150.0f,
                     "A4 - C5 - E5 - A5 - - - G5 - E5 - - - | F5 - - - E5 - C5 - - - A4 - C5 - - - | "
                     "D5 - - - G5 - - - B5 - A5 - G5 - D5 - | E5 - - - - - - - B4 - - - . . . . | "
                     "A5 - - - C6 - - - B5 - A5 - G5 - E5 - | F5 - A5 - C6 - - - A5 - F5 - C5 - - - | "
                     "G5 - B5 - D6 - - - C6 - B5 - G5 - - - | E5 - G5 - B5 - - - E6 - - - . . . .",
                     {Am, F, G, Em}, kDrive);
    case Tune::Factory: // D minor, hammering.
        return _song(152.0f,
                     "D5 - D5 . F5 - D5 . A5 - G5 - F5 - E5 - | F5 - - - D5 - Bb4 - - - D5 - F5 - - - | "
                     "E5 - - - G5 - C6 - - - Bb5 - A5 - G5 - | A5 - - - C#6 - - - E6 - - - . . . . | "
                     "D6 - - - A5 - F5 - D5 - F5 - A5 - D6 - | D6 - C6 - Bb5 - A5 - Bb5 - - - F5 - - - | "
                     "C6 - Bb5 - A5 - G5 - E5 - G5 - C6 - - - | C#6 - - - A5 - - - E5 - - - A4 - - -",
                     {Dm, Bb, C, A}, kDrive);
    case Tune::Storm: // E minor, restless.
        return _song(162.0f,
                     "E5 - G5 - B5 - E6 - D6 - B5 - G5 - E5 - | C6 - - - B5 - G5 - E5 - - - G5 - - - | "
                     "D6 - - - A5 - F#5 - D5 - F#5 - A5 - D6 - | D#6 - - - B5 - - - F#5 - - - . . . . | "
                     "E5 - G5 - B5 - E6 - D6 - B5 - G5 - E5 - | E6 - D6 - C6 - B5 - G5 - - - E5 - - - | "
                     "F#5 - A5 - D6 - - - C6 - A5 - F#5 - - - | B5 - - - D#6 - - - F#6 - - - B5 - - -",
                     {Em, C, D, B}, kDrive);
    case Tune::Ice: // B minor, glassy.
        return _song(136.0f,
                     "F#5 - - - B5 - - - D6 - C#6 - B5 - - - | G5 - - - B5 - D6 - G6 - - - F#6 - - - | "
                     "F#6 - E6 - D6 - A5 - F#5 - A5 - D6 - - - | E6 - - - C#6 - - - A5 - - - . . . .",
                     {Bm, G, D, A}, kSparse, true, Wave::Pulse12, 0.22f);
    case Tune::Forest: // G mixolydian, a bright march.
        return _song(144.0f,
                     "G5 - B5 - D6 - - - B5 - G5 - D5 - - - | F5 - A5 - C6 - - - A5 - F5 - C5 - - - | "
                     "E5 - G5 - C6 - E6 - D6 - C6 - G5 - - - | B5 - - - A5 - - - G5 - - - . . . .",
                     {G, F, C, G}, kLight);
    case Tune::Magma: // C minor, heavy.
        return _song(156.0f,
                     "C5 - C5 . Eb5 - G5 - C6 - - - Bb5 - G5 - | Ab5 - - - G5 - Eb5 - C5 - - - Eb5 - - - | "
                     "D5 - F5 - Bb5 - - - Ab5 - G5 - F5 - - - | G5 - - - B5 - - - D6 - - - . . . .",
                     {Cm, Ab, Bb, G}, kDrive);
    case Tune::Sky: // D major, soaring.
        return _song(148.0f,
                     "A5 - D6 - F#6 - - - E6 - D6 - A5 - - - | C#6 - E6 - A6 - - - G6 - E6 - C#6 - - - | "
                     "D6 - - - B5 - - - F#5 - - - B5 - D6 - | G5 - B5 - D6 - - - E6 - - - F#6 - - -",
                     {D, A, Bm, G}, kLight);
    case Tune::Fortress: // A minor, ominous, a held bass.
        return _song(140.0f,
                     "A4 - - - E5 - - - A5 - - - G#5 - - - | F5 - - - D5 - - - A4 - - - D5 - - - | "
                     "E5 - - - G#5 - - - B5 - - - D6 - - - | C6 - B5 - A5 - - - E5 - - - . . . .",
                     {Am, Dm, E, Am}, kSparse, false);
    case Tune::Boss: // E minor, urgent stabs.
        return _song(172.0f,
                     "E5 . E5 . G5 . E5 . B5 - A5 - G5 - F#5 - | E5 . E5 . G5 . C6 . B5 - - - G5 - - - | "
                     "D5 . D5 . F#5 . D5 . A5 - G5 - F#5 - D5 - | D#5 - - - F#5 - - - B5 - - - A5 - - -",
                     {Em, C, D, B}, kDrive);
    case Tune::FinalBoss: // D minor with a flat second, menacing.
        return _song(166.0f,
                     "D5 . D5 . A5 . D5 . F5 - E5 - D5 - A4 - | Eb5 . Eb5 . Bb5 . Eb5 . G5 - F5 - Eb5 - Bb4 - | "
                     "D6 - - - A5 - - - F5 - - - D5 - - - | C#5 - E5 - G5 - Bb5 - C#6 - - - . . . .",
                     {Dm, Eb, Dm, Csd}, kDrive);
    case Tune::WeaponGet: // G major, proud.
        return _song(120.0f,
                     "G5 - - - B5 - - - D6 - - - G6 - - - | E6 - - - C6 - - - G5 - - - E5 - - - | "
                     "F#5 - - - A5 - - - D6 - - - C6 - - - | B5 - - - - - - - G5 - - - - - - -",
                     {G, C, D, G}, kSparse, false);
    case Tune::Ending: // F major, slow and warm.
        return _song(100.0f,
                     "A5 - - - G5 - - - F5 - - - C5 - - - | E5 - - - G5 - - - C6 - - - - - - - | "
                     "D6 - - - C6 - - - A5 - - - F5 - - - | D5 - - - F5 - - - Bb5 - - - A5 - - -",
                     {F, C, Dm, Bb}, kSparse, false, Wave::Pulse25, 0.2f);
    case Tune::Count: break;
    }
    return {};
}

Tune stageTune(const game::Theme theme)
{
    switch (theme) {
    case game::Theme::Lab: return Tune::Lab;
    case game::Theme::Factory: return Tune::Factory;
    case game::Theme::Storm: return Tune::Storm;
    case game::Theme::Ice: return Tune::Ice;
    case game::Theme::Forest: return Tune::Forest;
    case game::Theme::Magma: return Tune::Magma;
    case game::Theme::Sky: return Tune::Sky;
    case game::Theme::Fortress: return Tune::Fortress;
    case game::Theme::Count: break;
    }
    return Tune::Lab;
}

} // namespace blaster::app
