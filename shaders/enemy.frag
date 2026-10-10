#version 450
#include "gamelib/sprite.glsl"
#include "gamelib/sdf.glsl"

// The twelve enemy kinds and the stage guardian, as SDF bodies with dark
// outlines. The quad is centered on the enemy's box, plus a margin.
//   color   body   color2  accent (eyes, lights, the guardian's armor)
//   params  x kind (0 crawler, 1 hopper, 2 diver, 3 shield walker, 4 turret,
//           5 nest, 6 drone, 7 roller, 8 lobber, 9 popper, 10 wisp,
//           11 dropper, 12 guardian), y facing (-1, 1), z animation phase
//           (or the turret's aim angle), w margin
//   bits.x  bit 0 open (shield down, hatch up), bit 1 hit flash, bit 2 frozen,
//           bit 3 in the air / airborne

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

const vec3 kInk = vec3(0.03, 0.02, 0.06);
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

void eye(vec2 p, float r, vec3 glow)
{
    part(sdCircle(p, r), vec3(1.0));
    fill(sdCircle(p - vec2(r * 0.3, 0.0), r * 0.5), glow * 0.3);
}

void main()
{
    SpriteData s = sprite;
    vec2 p = spriteLocal(inUv);
    int kind = int(s.params.x + 0.5);
    float facing = s.params.y < 0.0 ? -1.0 : 1.0;
    float phase = s.params.z;
    bool open = (s.bits.x & 1u) != 0u;
    bool flash = (s.bits.x & 2u) != 0u;
    bool frozen = (s.bits.x & 4u) != 0u;
    bool airborne = (s.bits.x & 8u) != 0u;
    vec3 body = s.color.rgb;
    vec3 accent = s.color2.rgb;
    vec2 q = vec2(p.x * facing, p.y); // facing +x
    float t = s.time;

    if (kind == 0) {
        // Crawler: a dome on wheels with an eye peeking under the rim.
        float spin = phase * 2.0 * kPi;
        for (int i = -1; i <= 1; i += 2) {
            vec2 w = q - vec2(float(i) * 4.0, 4.0);
            part(sdCircle(w, 2.4), vec3(0.25));
            fill(sdSegment(w, vec2(0.0), rot(vec2(1.6, 0.0), spin), 0.4), vec3(0.6));
        }
        part(max(sdEllipse(q - vec2(0.0, 1.0), vec2(7.4, 6.4)), q.y - 1.6), body);
        fill(sdRoundBox(q - vec2(3.4, 0.4), vec2(2.4, 1.0), 0.6), kInk);
        fill(sdCircle(q - vec2(4.2, 0.4), 0.7), accent);
    } else if (kind == 1) {
        // Hopper: a ball on a spring with one big eye.
        float squash = airborne ? 1.2 : 0.75;
        part(sdSegment(q, vec2(-2.0, 2.0), vec2(-2.0, 2.0 + 3.0 * squash), 1.0), vec3(0.6));
        part(sdSegment(q, vec2(2.0, 2.0), vec2(2.0, 2.0 + 3.0 * squash), 1.0), vec3(0.6));
        part(sdEllipse(q - vec2(0.0, -1.0), vec2(6.2, 5.6 / squash * 0.85)), body);
        eye(q - vec2(2.0, -1.6), 2.4, accent);
    } else if (kind == 2) {
        // Diver: a beaked flier with beating wings.
        float flap = sin(phase * 2.0 * kPi) * 0.6;
        vec2 w = rot(q - vec2(-1.0, -1.0), -0.3 + flap);
        part(sdEllipse(w - vec2(-3.0, -3.0), vec2(5.0, 1.8)), accent * 0.8);
        part(sdEllipse(q, vec2(6.0, 4.2)), body);
        part(sdSegment(q, vec2(5.0, 0.5), vec2(9.0, airborne ? 3.5 : 1.5), 1.1), vec3(1.0, 0.8, 0.3));
        eye(q - vec2(2.4, -1.4), 1.6, accent);
    } else if (kind == 3) {
        // Shield walker: a stubby robot behind a tall shield.
        float step_ = sin(phase * 2.0 * kPi) * 1.5;
        part(sdSegment(q, vec2(-2.0, 4.0), vec2(-2.0 - step_, 9.0), 1.4), vec3(0.3));
        part(sdSegment(q, vec2(2.0, 4.0), vec2(2.0 + step_, 9.0), 1.4), vec3(0.3));
        part(sdRoundBox(q - vec2(-1.0, -1.0), vec2(5.0, 6.0), 2.4), body);
        eye(q - vec2(1.4, -3.4), 1.8, accent);
        if (open) {
            part(sdRoundBox(q - vec2(5.0, 1.0), vec2(3.6, 1.6), 0.8), vec3(0.4)); // the gun out
            part(sdRoundBox(rot(q - vec2(4.0, 6.0), 1.2), vec2(1.4, 5.0), 1.0), accent); // shield lowered
        } else {
            part(sdRoundBox(q - vec2(6.0, 0.0), vec2(1.8, 9.0), 1.2), accent);
            fill(sdBox(q - vec2(6.0, 0.0), vec2(0.5, 6.0)), accent * 1.3);
        }
    } else if (kind == 4) {
        // Turret: an armored dome that opens to aim its barrel.
        vec2 b = q;
        if (open) {
            vec2 dir = vec2(cos(phase), sin(phase));
            dir.x *= facing;
            part(sdSegment(b, vec2(0.0, 0.0), dir * 9.0, 1.8), vec3(0.35));
            part(sdCircle(b - vec2(0.0, 0.5), 5.0), body);
            eye(b - vec2(0.0, -0.2), 2.0, accent);
        }
        float lid = open ? -4.0 : 0.0;
        part(max(sdEllipse(b - vec2(0.0, 2.0 + lid * 0.3), vec2(8.0, 6.8)), b.y - 2.5 - lid), accent * 0.7);
        part(sdRoundBox(b - vec2(0.0, 5.0), vec2(8.0, 2.0), 0.8), body * 0.7);
    } else if (kind == 5) {
        // Nest: a hive box with a glowing hatch.
        part(sdRoundBox(q, vec2(10.0, 10.0), 3.0), body);
        for (int i = 0; i < 3; ++i) {
            fill(sdBox(q - vec2(0.0, -6.0 + 4.0 * float(i)), vec2(8.5, 0.5)), body * 0.6);
        }
        float pulse = 0.5 + 0.5 * sin(t * 4.0 + phase * 6.0);
        acc = over(vec4(accent, glowFalloff(sdCircle(q - vec2(0.0, 1.0), 3.5), 3.0) * pulse), acc);
        part(sdCircle(q - vec2(0.0, 1.0), 3.5), mix(accent * 0.6, accent, pulse));
    } else if (kind == 6) {
        // Drone: an orb under a spinning rotor.
        float r = abs(sin(t * 30.0));
        part(sdSegment(q, vec2(-5.0 * r, -6.0), vec2(5.0 * r, -6.0), 0.7), vec3(0.7));
        fill(sdSegment(q, vec2(0.0, -6.0), vec2(0.0, -3.5), 0.6), vec3(0.4));
        part(sdCircle(q, 4.2), body);
        eye(q - vec2(1.2, 0.2), 1.6, accent);
    } else if (kind == 7) {
        // Roller: a spiked wheel.
        vec2 w = rot(q, phase * 2.0 * kPi);
        float a = atan(w.y, w.x);
        float spikes = length(w) - 6.0 - 1.6 * max(0.0, cos(a * 6.0));
        part(spikes, accent);
        part(sdCircle(w, 4.6), body);
        fill(sdCircle(w - vec2(2.0, 0.0), 1.2), kInk);
        fill(sdCircle(w + vec2(2.0, 0.0), 1.2), kInk);
    } else if (kind == 8) {
        // Lobber: a squat robot with a mortar on its back.
        part(sdSegment(q, vec2(-3.0, -2.0), vec2(-6.0, -8.0), 2.2), vec3(0.4));
        part(sdRoundBox(q - vec2(0.0, 2.0), vec2(7.0, 6.0), 3.0), body);
        part(sdRoundBox(q - vec2(0.0, 8.0), vec2(7.0, 1.6), 1.0), vec3(0.3));
        eye(q - vec2(3.0, 0.6), 1.9, accent);
    } else if (kind == 9) {
        // Popper: a helmet hiding in the floor, popping up to fire.
        float rise = open ? 0.0 : 5.0;
        if (open) {
            part(sdRoundBox(q - vec2(0.0, 3.0), vec2(5.0, 4.0), 2.0), body * 0.8);
            eye(q - vec2(1.6, 2.0), 1.7, accent);
        }
        part(max(sdEllipse(q - vec2(0.0, -1.0 + rise), vec2(8.0, 6.0)), q.y - 0.6 - rise), accent);
        fill(sdBox(q - vec2(0.0, -2.0 + rise), vec2(6.5, 0.6)), accent * 0.6);
    } else if (kind == 10) {
        // Wisp: a flame orb with a flickering tail.
        float flick = sin(t * 18.0 + phase * 10.0) * 0.8;
        float tail = sdSegment(q, vec2(0.0, 0.0), vec2(-9.0, flick), 2.2);
        acc = over(vec4(accent, glowFalloff(min(tail, sdCircle(q, 4.6)), 4.0) * 0.6), acc);
        fill(tail, accent);
        part(sdCircle(q, 4.6), body);
        fill(sdCircle(q - vec2(1.8, -0.8), 1.0), kInk);
    } else if (kind == 11) {
        // Dropper: a bat-like bot, wings folded while it hangs.
        float spread = airborne ? 1.0 : 0.15;
        float flap = airborne ? sin(t * 14.0) * 0.4 : 0.0;
        for (int i = -1; i <= 1; i += 2) {
            vec2 w = rot(vec2(q.x * float(i), q.y), -0.4 * spread + flap);
            part(sdEllipse(w - vec2(4.0 + 3.0 * spread, -1.0), vec2(3.0 + 3.0 * spread, 1.8)), accent * 0.7);
        }
        part(sdCircle(q, 4.6), body);
        eye(q - vec2(1.6, 0.4), 1.4, accent);
        fill(sdSegment(q, vec2(0.0, -4.6), vec2(0.0, -7.0), 0.6), vec3(0.4));
    } else {
        // The guardian: an armored walker with treads and a big cannon.
        float bob = sin(t * 3.0) * 0.6;
        part(sdRoundBox(q - vec2(0.0, 13.0), vec2(15.0, 4.0), 3.5), vec3(0.2));
        for (int i = -2; i <= 2; ++i) {
            fill(sdCircle(q - vec2(float(i) * 6.0, 13.0), 2.2), vec3(0.45));
        }
        part(sdRoundBox(q - vec2(10.0, -2.0 + bob), vec2(9.0, 3.4), 1.6), vec3(0.32)); // the cannon
        fill(sdCircle(q - vec2(18.0, -2.0 + bob), 2.0), kInk);
        part(sdRoundBox(q - vec2(-2.0, 1.0 + bob), vec2(13.0, 9.0), 4.0), body);
        part(sdRoundBox(q - vec2(-2.0, -9.0 + bob), vec2(8.0, 4.0), 3.0), accent);
        fill(sdRoundBox(q - vec2(1.0, -9.0 + bob), vec2(4.0, 1.4), 0.8), kInk);
        float glow = 0.6 + 0.4 * sin(t * 6.0);
        fill(sdRoundBox(q - vec2(2.0, -9.0 + bob), vec2(2.4, 0.6), 0.4), vec3(1.0, 0.3, 0.2) * (0.8 + glow));
        fill(sdBox(q - vec2(-2.0, 4.0 + bob), vec2(11.0, 0.8)), accent * 0.7);
    }

    vec3 rgb = acc.rgb;
    if (frozen) {
        rgb = mix(rgb, vec3(0.75, 0.92, 1.0), 0.6);
    }
    if (flash) {
        rgb = mix(rgb, vec3(1.0), 0.75);
    }
    outColor = vec4(rgb, acc.a * s.color.a);
}
