#version 450
#include "gamelib/sprite.glsl"
#include "gamelib/sdf.glsl"
#include "gamelib/noise.glsl"

// Every projectile: the hero's buster shots (small, half and full charge)
// and special weapons, the enemies' pellets, bombs and orbs, and the bosses'
// sparks, icicles, seeds, fire, feathers, lasers, lightning and thorns.
//   color   main   color2  glow
//   params  x look (0 buster, 1 half charge, 2 full charge, 3 saw, 4 spark,
//           5 ice lance, 6 leaf, 7 magma, 8 gale wing, 10 pellet, 11 bomb,
//           12 orb, 13 spark, 14 icicle, 15 seed, 16 fire, 17 feather,
//           18 laser, 19 lightning, 20 thorn, 21 debris, 22 blast, 23 saw),
//           y heading (radians), z age in ticks, w glow margin

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

const float kPi = 3.14159265;
const vec3 kInk = vec3(0.04, 0.03, 0.06);

vec4 acc = vec4(0.0);

void fill(float d, vec3 rgb)
{
    acc = over(vec4(rgb, coverage(d)), acc);
}

void part(float d, vec3 rgb)
{
    acc = over(vec4(kInk, coverage(d - 0.7)), acc);
    acc = over(vec4(rgb, coverage(d)), acc);
}

void glow(float d, vec3 rgb, float width, float strength)
{
    acc = over(vec4(rgb, glowFalloff(d, width) * strength), acc);
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
    int look = int(s.params.x + 0.5);
    float heading = s.params.y;
    float age = s.params.z;
    vec3 c1 = s.color.rgb;
    vec3 c2 = s.color2.rgb;
    float t = s.time;
    vec2 q = rot(p, -heading); // along the flight: +x forward
    vec2 half_ = s.size * 0.5 - s.params.w;

    if (look == 0) {
        float d = sdEllipse(q, vec2(3.4, 2.6));
        glow(d, c2, 2.0, 0.6);
        fill(d, c1);
        fill(sdEllipse(q - vec2(0.6, -0.6), vec2(1.4, 0.9)), vec3(1.0));
    } else if (look == 1) {
        float d = sdEllipse(q, vec2(5.4, 4.2));
        glow(d, c2, 3.0, 0.7);
        fill(d, c1);
        fill(sdEllipse(q - vec2(1.0, -0.8), vec2(2.4, 1.6)), vec3(1.0));
    } else if (look == 2) {
        // The full charge: a comet with a flickering tail.
        float tail = sdSegment(q, vec2(-16.0, 0.0), vec2(0.0, 0.0), 2.5 + 3.5 * (q.x + 16.0) / 16.0);
        float head = sdCircle(q - vec2(2.0, 0.0), 7.0);
        float wob = 0.6 * sin(t * 40.0 + q.x);
        glow(min(tail, head), c2, 5.0, 0.8);
        fill(tail + wob, c2);
        fill(head, c1);
        fill(sdCircle(q - vec2(3.5, -1.5), 3.0), vec3(1.0));
    } else if (look == 3 || look == 23) {
        // A spinning saw blade.
        vec2 r = rot(p, t * 18.0);
        float teeth = length(r) - 5.6 - 1.4 * step(0.0, sin(atan(r.y, r.x) * 10.0));
        part(teeth, c1);
        fill(sdCircle(r, 2.2), c2);
        fill(sdCircle(r, 0.9), kInk);
    } else if (look == 4 || look == 13) {
        // A crackling spark ball.
        float n = valueNoise(p * 0.8 + vec2(t * 8.0, -t * 6.0));
        float d = length(p) - 3.8 - 1.4 * n;
        glow(d, c2, 4.0, 0.9);
        fill(d, c1);
        for (int i = 0; i < 3; ++i) {
            float a = t * 20.0 + float(i) * 2.1;
            fill(sdSegment(p, vec2(0.0), vec2(cos(a), sin(a)) * 7.0, 0.4), vec3(1.0));
        }
    } else if (look == 5 || look == 14) {
        // An ice lance or an icicle: a long crystal.
        float d = max(abs(q.y) - 2.6 * (1.0 - abs(q.x) / max(half_.x, 1.0)), abs(q.x) - half_.x);
        glow(d, c2, 2.5, 0.6);
        fill(d, c1);
        fill(sdSegment(q, vec2(-half_.x * 0.6, -0.6), vec2(half_.x * 0.5, -0.6), 0.4), vec3(1.0));
    } else if (look == 6) {
        // A spinning leaf.
        vec2 r = rot(p, t * 9.0);
        float d = sdEllipse(r, vec2(4.6, 2.2));
        part(d, c1);
        fill(sdSegment(r, vec2(-4.0, 0.0), vec2(4.0, 0.0), 0.35), c2);
    } else if (look == 7 || look == 16) {
        // A ball (or a wave) of fire.
        float n = valueNoise(vec2(p.x * 0.6, p.y * 0.6 + t * 10.0));
        float d = look == 7 ? length(p) - 4.6 - 1.2 * n : sdEllipse(p - vec2(0.0, 1.0), vec2(half_.x, half_.y - 1.0)) - 1.6 * n;
        glow(d, c2, 4.0, 0.8);
        fill(d, c2);
        fill(d + 2.0, c1);
        fill(d + 3.5, vec3(1.0, 0.95, 0.7));
    } else if (look == 8 || look == 17) {
        // A gale wing or a feather: a crescent.
        float a = sdCircle(q - vec2(-1.0, 0.0), 5.0);
        float b = sdCircle(q - vec2(-3.5, 0.0), 4.6);
        float d = max(a, -b);
        if (look == 17) {
            d = sdEllipse(q, vec2(4.6, 1.6));
            fill(sdSegment(q, vec2(-5.0, 0.0), vec2(4.0, 0.0), 0.3), c2);
        }
        glow(d, c2, 2.5, 0.5);
        part(d, c1);
    } else if (look == 10 || look == 12) {
        float r = look == 12 ? 4.6 : 3.0;
        float d = sdCircle(p, r);
        glow(d, c2, 2.5, 0.7);
        fill(d, c1);
        fill(sdCircle(p - vec2(-r * 0.3), r * 0.4), vec3(1.0));
    } else if (look == 11) {
        // A bomb with a fizzing fuse.
        part(sdCircle(p - vec2(0.0, 0.8), 4.4), vec3(0.18, 0.18, 0.22));
        fill(sdSegment(p, vec2(1.0, -3.0), vec2(2.4, -5.4), 0.6), vec3(0.6, 0.5, 0.3));
        float spark = 0.5 + 0.5 * sin(t * 40.0);
        glow(sdCircle(p - vec2(2.6, -5.8), 0.8), c2, 2.0, spark);
        fill(sdCircle(p - vec2(-1.2, -0.6), 1.0), vec3(0.6));
    } else if (look == 15) {
        // A seed pod.
        vec2 r = rot(p, age * 0.2);
        part(sdEllipse(r, vec2(4.0, 2.8)), c1);
        fill(sdSegment(r, vec2(-3.0, 0.0), vec2(3.0, 0.0), 0.4), c2);
    } else if (look == 18 || look == 19) {
        // A laser across the room or a lightning bolt down it.
        vec2 a = look == 18 ? p : p.yx;
        float len = look == 18 ? half_.x : half_.y;
        float width = look == 18 ? half_.y : half_.x;
        float zig = look == 19 ? 3.0 * (valueNoise(vec2(a.x * 0.08, t * 20.0)) - 0.5) * 2.0 : 0.0;
        float fade = clamp(age / 3.0, 0.0, 1.0);
        float d = max(abs(a.y - zig) - width * (0.6 + 0.2 * sin(t * 50.0)), abs(a.x) - len);
        glow(d, c2, 6.0, 0.8 * fade);
        fill(d, c1 * fade + vec3(1.0) * (1.0 - fade));
        fill(d + width * 0.4, vec3(1.0));
    } else if (look == 20) {
        // A thorn bursting from the floor.
        float grow = clamp(age / 6.0, 0.0, 1.0);
        float top = half_.y - 2.0 * half_.y * grow;
        float d = max(abs(p.x) - 5.0 * (p.y - top) / (2.0 * half_.y), top - p.y);
        part(max(d, p.y - half_.y), c1);
        fill(max(sdSegment(p, vec2(-1.0, top + 6.0), vec2(-1.0, half_.y), 0.6), -d), c2);
    } else if (look == 21) {
        // Falling debris.
        vec2 r = rot(p, age * 0.15);
        part(sdRoundBox(r, vec2(4.0, 3.0), 1.0), c1);
        fill(sdSegment(r, vec2(-2.0, -1.0), vec2(1.5, 1.2), 0.4), c2);
    } else if (look == 22) {
        // A blast.
        float k = clamp(age / 10.0, 0.0, 1.0);
        float d = length(p) - min(half_.x, half_.y) * (0.4 + 0.6 * k);
        glow(d, c2, 4.0, 1.0 - k);
        fill(d, mix(vec3(1.0, 0.95, 0.7), c1, k) * (1.0 - k * 0.5));
    }
    outColor = vec4(acc.rgb, acc.a * s.color.a);
}
