#version 450
#include "gamelib/sprite.glsl"
#include "gamelib/sdf.glsl"

// Rivet, the hero: a small robot in a round helmet with a fin, a visor and
// an arm cannon, drawn from SDF parts with dark outlines and recolored by
// the equipped weapon. The quad is the body plus a margin, feet at its
// bottom (minus the margin).
//   color   helmet, arms and boots (the weapon's main color)
//   color2  suit and fin (the weapon's trim color)
//   params  x facing (-1, 1), y pose (0 stand, 1 run, 2 jump, 3 climb,
//           4 slide, 5 hurt, 6 beam), z run or climb phase 0..1, w margin
//   bits.x  bit 0 shooting (cannon out), bits 8..15 charge 0..255,
//           bit 16 charge full (flashing)

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

const vec3 kInk = vec3(0.03, 0.03, 0.07);
const vec3 kFace = vec3(1.0, 0.86, 0.72);
const float kPi = 3.14159265;

vec4 acc = vec4(0.0);

void part(float d, vec3 rgb)
{
    acc = over(vec4(kInk, coverage(d - 0.8)), acc);
    acc = over(vec4(rgb, coverage(d)), acc);
}

void fill(float d, vec3 rgb)
{
    acc = over(vec4(rgb, coverage(d)), acc);
}

vec2 rot(vec2 p, float a)
{
    float c = cos(a);
    float s = sin(a);
    return vec2(c * p.x - s * p.y, s * p.x + c * p.y);
}

void main()
{
    SpriteData s = sprite;
    vec2 p = spriteLocal(inUv);
    float margin = s.params.w;
    float height = s.size.y - 2.0 * margin;
    float facing = s.params.x < 0.0 ? -1.0 : 1.0;
    int pose = int(s.params.y + 0.5);
    float phase = s.params.z;
    bool shooting = (s.bits.x & 1u) != 0u;
    float charge = float((s.bits.x >> 8) & 0xffu) / 255.0;
    bool full = (s.bits.x & 0x10000u) != 0u;
    vec3 main = s.color.rgb;
    vec3 trim = s.color2.rgb;

    // Feet at y = 0, up negative, facing +x.
    vec2 q = vec2(p.x * facing, p.y - height * 0.5);

    if (pose == 6) {
        // Beaming: a bright column.
        float beam = sdRoundBox(q - vec2(0.0, -height * 0.5), vec2(2.4, height * 0.5), 2.4);
        acc = over(vec4(trim * 1.4, glowFalloff(beam, 3.0) * 0.6), acc);
        fill(beam, mix(trim, vec3(1.0), 0.6));
        outColor = vec4(acc.rgb, acc.a * s.color.a);
        return;
    }

    // Charging: the outline shifts towards white and an aura pulses.
    float pulse = 0.5 + 0.5 * sin(s.time * (full ? 40.0 : 22.0));
    if (charge > 0.0) {
        main = mix(main, vec3(1.0), charge * pulse * (full ? 0.55 : 0.25));
    }

    if (pose == 4) {
        // Sliding: tipped forward, low to the ground.
        q = rot(q - vec2(-2.0, 0.0), -0.95) + vec2(0.0, 4.0);
    } else if (pose == 5) {
        q = rot(q - vec2(0.0, -10.0), 0.35) + vec2(0.0, -10.0); // knocked back
    } else if (pose == 1) {
        q.y += abs(sin(phase * 2.0 * kPi)) * -0.8; // a bounce in the stride
    }

    float swing = pose == 1 ? sin(phase * 2.0 * kPi) * 3.2 : 0.0;
    float lift = pose == 1 ? max(0.0, cos(phase * 2.0 * kPi)) * 1.6 : 0.0;
    vec2 backFoot = vec2(-2.2 - swing, -1.6 - (pose == 1 ? max(0.0, -cos(phase * 2.0 * kPi)) * 1.6 : 0.0));
    vec2 frontFoot = vec2(2.6 + swing, -1.6 - lift);
    if (pose == 2) {
        backFoot = vec2(-3.6, -3.4);
        frontFoot = vec2(3.4, -5.6);
    } else if (pose == 3) {
        float c = sin(phase * 2.0 * kPi);
        backFoot = vec2(-2.4, -1.6 - max(0.0, c) * 3.0);
        frontFoot = vec2(2.4, -1.6 - max(0.0, -c) * 3.0);
    } else if (pose == 5) {
        backFoot = vec2(-4.0, -2.4);
        frontFoot = vec2(3.8, -1.4);
    }
    vec2 hip = vec2(0.0, -8.0);
    vec3 suitDark = trim * 0.65;

    bool back = pose == 3 && !shooting; // on a ladder: seen from behind
    // Back leg and arm.
    part(sdSegment(q, hip + vec2(-1.4, 0.0), backFoot + vec2(0.0, -1.0), 1.9), suitDark);
    part(sdEllipse(q - backFoot, vec2(3.2, 2.2)), main * 0.8);
    vec2 backHand = vec2(-4.6 + swing * 0.4, -10.2);
    if (pose == 2) {
        backHand = vec2(-5.4, -15.0);
    } else if (pose == 3) {
        backHand = vec2(-3.6, -15.5 - max(0.0, sin(phase * 2.0 * kPi)) * 4.0);
    } else if (pose == 5) {
        backHand = vec2(-6.8, -16.0);
    }
    if (!back) {
        part(sdSegment(q, vec2(-3.0, -13.5), backHand, 1.9), main * 0.8);
    }

    // Torso, belt, chest light.
    part(sdRoundBox(q - vec2(0.0, -12.2), vec2(4.6, 4.4), 2.2), trim);
    fill(sdBox(q - vec2(0.0, -8.6), vec2(4.4, 0.9)), main * 0.75);
    if (!back) {
        float core = length(q - vec2(1.6, -13.0)) - 1.4;
        vec3 coreColor = mix(vec3(0.9, 1.0, 1.0), vec3(1.0), charge);
        acc = over(vec4(coreColor, glowFalloff(core, 2.0 + 3.0 * charge) * (0.4 + 0.6 * charge)), acc);
        fill(core, coreColor);
    } else {
        fill(sdRoundBox(q - vec2(0.0, -13.0), vec2(2.6, 2.6), 1.0), main * 0.8); // backpack
    }

    // Front leg.
    part(sdSegment(q, hip + vec2(1.4, 0.0), frontFoot + vec2(0.0, -1.0), 1.9), suitDark * 1.1);
    part(sdEllipse(q - frontFoot, vec2(3.4, 2.3)), main);

    // Head: helmet, fin, ear bolt, face and visor.
    vec2 h = q - vec2(0.4, -19.4);
    part(sdSegment(h, vec2(-4.6, -3.0), vec2(-0.6, -6.6), 1.5), trim); // the fin
    part(sdCircle(h, 5.8), main);
    if (back) {
        fill(sdCircle(h - vec2(0.0, 0.5), 2.0), trim * 0.8);
    } else {
        fill(sdCircle(h - vec2(-1.8, 0.3), 1.7), trim);
        part(sdRoundBox(h - vec2(2.6, 1.2), vec2(2.8, 2.9), 1.6), kFace);
        // The visor: a dark band with a glowing slit.
        fill(sdRoundBox(h - vec2(2.8, -0.6), vec2(3.0, 1.15), 0.8), kInk);
        if (pose == 5) {
            fill(sdSegment(h, vec2(2.0, -1.4), vec2(4.2, 0.2), 0.35), vec3(1.0, 0.4, 0.4));
            fill(sdSegment(h, vec2(2.0, 0.2), vec2(4.2, -1.4), 0.35), vec3(1.0, 0.4, 0.4));
        } else {
            fill(sdRoundBox(h - vec2(3.4, -0.6), vec2(1.6, 0.35), 0.3), mix(trim, vec3(1.0), 0.6));
        }
        fill(sdSegment(h, vec2(1.6, 2.8), vec2(3.6, 2.8), 0.3), kInk * 2.0); // mouth line
    }

    // Front arm: the cannon out when shooting, else a fist.
    if (shooting) {
        part(sdRoundBox(q - vec2(5.6, -12.4), vec2(4.6, 2.7), 1.6), main);
        fill(sdCircle(q - vec2(10.0, -12.4), 1.5), kInk);
        fill(sdBox(q - vec2(4.2, -12.4), vec2(0.5, 2.7)), trim);
    } else if (back) {
        vec2 hand = vec2(3.6, -15.5 - max(0.0, -sin(phase * 2.0 * kPi)) * 4.0);
        part(sdSegment(q, vec2(3.0, -13.5), hand, 2.0), main);
    } else {
        vec2 hand = vec2(3.6 - swing * 0.4, -10.0);
        if (pose == 2) {
            hand = vec2(5.2, -17.6);
        } else if (pose == 5) {
            hand = vec2(6.4, -17.0);
        }
        part(sdSegment(q, vec2(2.6, -13.6), hand, 2.0), main);
        part(sdCircle(q - hand, 2.4), main);
    }

    if (charge > 0.3) {
        // The charge aura around the whole body.
        float body = sdRoundBox(q - vec2(0.0, -12.0), vec2(6.0, 11.0), 6.0);
        acc = over(acc, vec4(mix(trim, vec3(1.0), 0.5), glowFalloff(body, 4.0) * charge * pulse * 0.5));
    }
    outColor = vec4(acc.rgb, acc.a * s.color.a);
}
