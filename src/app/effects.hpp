#pragma once

// Presentation-only juice, on real frame time: muzzle flashes, impact
// sparks, explosion rings and debris when anything dies, the hero's and the
// bosses' death bursts of orbs, dust on landings and slides, the screen
// shake and the white flash. World units (drawn under the stage's view)
// except the flash and the shake (canvas units).

#include "session.hpp"

#include <gamelib/core/color.hpp>
#include <gamelib/core/math.hpp>
#include <gamelib/core/rng.hpp>
#include <gamelib/draw/draw_list.hpp>
#include <gamelib/fx/juice.hpp>
#include <gamelib/fx/particles.hpp>
#include <gamelib/fx/puffs.hpp>
#include <gamelib/fx/transients.hpp>

namespace blaster::app {

class Effects {
public:
    Effects();

    void onEvent(const game::Event& event, game::Theme theme);
    void update(float seconds);
    void clear();
    // A ring and orbs for the ending's fireworks and the weapon-get scene.
    void burst(gamelib::core::Vec2 at, gamelib::core::Color color, float size);

    void drawWorld(gamelib::draw::DrawList& list) const;
    void drawFlash(gamelib::draw::DrawList& list, gamelib::core::Size canvas) const;
    [[nodiscard]] gamelib::core::Vec2 shake(float time) const { return m_shake.offset(time); }
    // 0..1 while the hero was just hurt (a red tint on the HUD).
    [[nodiscard]] float hurt() const { return m_hurt.value(); }

private:
    struct Flash {
        gamelib::core::Vec2 at;
        gamelib::core::Color color;
        float size = 8.0f;
    };
    // The classic death: orbs flying out in eight directions at two speeds.
    struct Orbs {
        gamelib::core::Vec2 at;
        gamelib::core::Color color;
        float speed = 60.0f;
    };

    gamelib::fx::Particles m_debris;
    gamelib::fx::Puffs m_dust;
    gamelib::fx::Rings m_rings;
    gamelib::fx::Sparks m_sparks;
    gamelib::fx::Transients<Flash> m_flashes;
    gamelib::fx::Transients<Orbs> m_orbs;
    gamelib::fx::Fade m_flash{0.3f};
    gamelib::fx::Fade m_hurt{0.4f};
    gamelib::fx::Shake m_shake{0.35f, 18.0f};
    gamelib::core::Rng m_rng;
};

} // namespace blaster::app
