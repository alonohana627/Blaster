#version 450
#include "gamelib/sprite.glsl"
#include "gamelib/sdf.glsl"

// Pickups and HUD icons: health capsules, weapon energy cells, the extra
// life (the hero's head), the energy tank, the checkpoint beacon and the
// weapon icons of the pause menu.
//   color   main   color2  light
//   params  x kind (0 small health, 1 big health, 2 small energy, 3 big
//           energy, 4 extra life, 5 tank, 6 beacon, 7 weapon icon),
//           y blink 0..1 (fades out), z lit (beacon) or weapon (icon), w margin

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

const vec3 kInk = vec3(0.03, 0.03, 0.07);

vec4 acc = vec4(0.0);

void part(float d, vec3 rgb)
{
    acc = over(vec4(kInk, coverage(d - 0.7)), acc);
    acc = over(vec4(rgb, coverage(d)), acc);
}

void fill(float d, vec3 rgb)
{
    acc = over(vec4(rgb, coverage(d)), acc);
}

void glow(float d, vec3 rgb, float width, float strength)
{
    acc = over(vec4(rgb, glowFalloff(d, width) * strength), acc);
}

void main()
{
    SpriteData s = sprite;
    vec2 p = spriteLocal(inUv);
    int kind = int(s.params.x + 0.5);
    vec3 c1 = s.color.rgb;
    vec3 c2 = s.color2.rgb;
    float t = s.time;
    float shine = 0.5 + 0.5 * sin(t * 6.0);

    if (kind == 0 || kind == 1) {
        // A health capsule: two halves and a bright band.
        float k = kind == 1 ? 1.6 : 1.0;
        float body = sdRoundBox(p, vec2(4.0, 2.8) * k, 2.6 * k);
        glow(body, c2, 2.0 * k, 0.3 * shine);
        part(body, c1);
        fill(max(body, p.x), vec3(1.0));
        fill(sdBox(p, vec2(0.6, 2.8) * k), c2);
    } else if (kind == 2 || kind == 3) {
        // A weapon energy cell: a capsule standing up with a bolt.
        float k = kind == 3 ? 1.6 : 1.0;
        float body = sdRoundBox(p, vec2(2.8, 4.0) * k, 1.4 * k);
        glow(body, c2, 2.0 * k, 0.3 * shine);
        part(body, c1);
        fill(sdSegment(p, vec2(0.6, -2.4) * k, vec2(-0.8, 0.2) * k, 0.5 * k), vec3(1.0));
        fill(sdSegment(p, vec2(-0.8, 0.2) * k, vec2(0.8, 0.0) * k, 0.5 * k), vec3(1.0));
        fill(sdSegment(p, vec2(0.8, 0.0) * k, vec2(-0.6, 2.4) * k, 0.5 * k), vec3(1.0));
    } else if (kind == 4) {
        // An extra life: the hero's helmeted head.
        part(sdSegment(p, vec2(-4.2, -3.0), vec2(-0.8, -6.0), 1.3), c2);
        part(sdCircle(p, 5.4), c1);
        part(sdRoundBox(p - vec2(2.0, 1.2), vec2(2.6, 2.6), 1.4), vec3(1.0, 0.86, 0.72));
        fill(sdRoundBox(p - vec2(2.2, -0.4), vec2(2.8, 1.0), 0.7), kInk);
        fill(sdRoundBox(p - vec2(2.8, -0.4), vec2(1.4, 0.3), 0.3), mix(c2, vec3(1.0), 0.6));
    } else if (kind == 5) {
        // An energy tank: a box marked with an E.
        float body = sdRoundBox(p, vec2(6.0, 6.0), 1.6);
        glow(body, c2, 3.0, 0.4 * shine);
        part(body, c1);
        fill(sdBox(p - vec2(-1.4, 0.0), vec2(0.8, 3.6)), vec3(1.0));
        fill(sdBox(p - vec2(0.6, -3.0), vec2(2.6, 0.7)), vec3(1.0));
        fill(sdBox(p - vec2(0.2, 0.0), vec2(2.2, 0.7)), vec3(1.0));
        fill(sdBox(p - vec2(0.6, 3.0), vec2(2.6, 0.7)), vec3(1.0));
    } else if (kind == 6) {
        // A checkpoint beacon: a post with a lamp, lit once reached.
        bool lit = s.params.z > 0.5;
        part(sdRoundBox(p - vec2(0.0, 6.0), vec2(1.2, 9.0), 0.6), vec3(0.55, 0.58, 0.68));
        part(sdRoundBox(p - vec2(0.0, 15.0), vec2(4.0, 1.4), 0.6), vec3(0.35, 0.38, 0.48));
        float lamp = sdCircle(p - vec2(0.0, -4.5), 3.4);
        if (lit) {
            glow(lamp, c2, 6.0, 0.5 + 0.3 * shine);
        }
        part(lamp, lit ? c2 : vec3(0.3, 0.32, 0.4));
    } else {
        // A weapon icon: a disc in the weapon's colors with a mark.
        int w = int(s.params.z + 0.5);
        float disc = sdCircle(p, 7.0);
        part(disc, c1);
        if (w == 0) {
            fill(sdEllipse(p, vec2(3.4, 2.4)), c2);
        } else if (w == 1) {
            fill(length(p) - 3.6 - 1.0 * step(0.0, sin(atan(p.y, p.x) * 8.0)), c2);
        } else if (w == 2) {
            fill(sdSegment(p, vec2(1.0, -4.0), vec2(-1.5, 0.4), 0.8), c2);
            fill(sdSegment(p, vec2(-1.5, 0.4), vec2(1.5, -0.4), 0.8), c2);
            fill(sdSegment(p, vec2(1.5, -0.4), vec2(-1.0, 4.0), 0.8), c2);
        } else if (w == 3) {
            fill(max(abs(p.y) - 1.6 * (1.0 - abs(p.x) / 5.0), abs(p.x) - 5.0), c2);
        } else if (w == 4) {
            fill(sdEllipse(p, vec2(4.6, 2.2)), c2);
        } else if (w == 5) {
            fill(length(p) - 3.6, c2);
        } else {
            fill(max(sdCircle(p - vec2(-0.8, 0.0), 4.2), -sdCircle(p - vec2(-2.8, 0.0), 3.8)), c2);
        }
    }
    float blink = s.params.y;
    outColor = vec4(acc.rgb, acc.a * s.color.a * (1.0 - blink));
}
