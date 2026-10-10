#version 450
#include "gamelib/sprite.glsl"
#include "gamelib/sdf.glsl"

// The six robot masters and the Overseer as layered SDF bodies (also their
// portraits on the stage select). The quad is centered on the boss's box,
// plus a margin.
//   color   body   color2  glow and trim
//   params  x boss (0 Geargrinder, 1 Stormcaller, 2 Rimefang, 3 Thornwood,
//           4 Cindermaw, 5 Galewing, 6 Overseer), y facing (-1, 1),
//           z pose (0 idle, 1 winding up, 2 moving or in the air, 3 attacking),
//           w margin
//   bits.x  bit 0 angry (below half health), bit 1 hit flash, bit 2 furious
//           (below a quarter)

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

const vec3 kInk = vec3(0.03, 0.02, 0.06);
const float kPi = 3.14159265;

vec4 acc = vec4(0.0);

void part(float d, vec3 rgb)
{
    acc = over(vec4(kInk, coverage(d - 0.9)), acc);
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

vec2 rot(vec2 p, float a)
{
    float c = cos(a);
    float s = sin(a);
    return vec2(c * p.x - s * p.y, s * p.x + c * p.y);
}

void eyes(vec2 q, vec2 at, vec3 color, float angry)
{
    fill(sdRoundBox(q - at, vec2(3.6, 1.5), 0.8), kInk);
    float slant = angry * 0.5;
    fill(sdRoundBox(rot(q - at - vec2(1.2, 0.0), slant), vec2(1.4, 0.55), 0.3), color);
    glow(sdCircle(q - at - vec2(1.2, 0.0), 1.0), color, 2.5, 0.5);
}

void main()
{
    SpriteData s = sprite;
    vec2 p = spriteLocal(inUv);
    int kind = int(s.params.x + 0.5);
    float facing = s.params.y < 0.0 ? -1.0 : 1.0;
    int pose = int(s.params.z + 0.5);
    bool angry = (s.bits.x & 1u) != 0u;
    bool flash = (s.bits.x & 2u) != 0u;
    bool furious = (s.bits.x & 4u) != 0u;
    vec3 body = s.color.rgb;
    vec3 trim = s.color2.rgb;
    vec2 q = vec2(p.x * facing, p.y);
    float t = s.time;
    float rage = angry ? (furious ? 1.0 : 0.5) : 0.0;
    vec3 eyeColor = mix(vec3(1.0, 0.95, 0.6), vec3(1.0, 0.2, 0.2), rage);

    if (kind == 0) {
        // GEARGRINDER: a stocky foundry robot with a saw in its chest.
        float spin = t * (pose == 1 ? 14.0 : 4.0);
        part(sdRoundBox(q - vec2(-5.0, 11.0), vec2(4.0, 4.0), 1.6), body * 0.7);
        part(sdRoundBox(q - vec2(5.0, 11.0), vec2(4.0, 4.0), 1.6), body * 0.7);
        part(sdRoundBox(q - vec2(-10.0, 0.0), vec2(4.0, 6.5), 2.5), body * 0.85); // back arm
        part(sdRoundBox(q - vec2(0.0, 1.0), vec2(11.0, 10.0), 4.0), body);
        vec2 g = rot(q - vec2(1.0, 2.0), spin);
        float teeth = length(g) - 6.0 - 1.4 * step(0.0, sin(atan(g.y, g.x) * 8.0));
        part(teeth, trim);
        fill(sdCircle(g, 2.0), kInk);
        part(sdRoundBox(q - vec2(1.0, -12.0), vec2(6.0, 4.6), 2.4), body * 1.1);
        eyes(q, vec2(3.0, -12.0), eyeColor, rage);
        vec2 arm = pose == 1 ? vec2(10.0, -10.0) : vec2(11.0, 1.0);
        part(sdRoundBox(q - arm, vec2(3.6, 6.0), 2.4), body * 0.95);
        if (pose == 1) {
            vec2 h = rot(q - arm - vec2(0.0, -8.0), spin);
            part(length(h) - 4.5 - 1.0 * step(0.0, sin(atan(h.y, h.x) * 8.0)), trim);
        }
    } else if (kind == 1) {
        // STORMCALLER: a hooded caster floating on a thundercloud.
        float bob = sin(t * 2.0) * 1.0;
        q.y -= bob;
        float cloud = min(min(sdCircle(q - vec2(-6.0, 11.0), 5.0), sdCircle(q - vec2(0.0, 12.5), 6.0)), sdCircle(q - vec2(6.0, 11.0), 5.0));
        part(cloud, mix(body * 0.6, vec3(0.5, 0.5, 0.65), 0.5));
        part(sdRoundBox(q - vec2(0.0, 1.0), vec2(9.0, 9.0), 4.0), body);
        fill(sdBox(q - vec2(0.0, 2.0), vec2(1.0, 7.0)), trim * 0.8); // a stripe of the robe
        part(sdCircle(q - vec2(0.0, -11.0), 6.5), body * 1.15);
        // Lightning-rod horns.
        part(sdSegment(q, vec2(-4.0, -15.0), vec2(-8.0, -21.0), 1.2), trim);
        part(sdSegment(q, vec2(4.0, -15.0), vec2(8.0, -21.0), 1.2), trim);
        float zap = 0.5 + 0.5 * sin(t * 30.0);
        glow(sdCircle(q - vec2(-8.0, -21.0), 1.0), trim, 3.0, 0.6 * zap);
        glow(sdCircle(q - vec2(8.0, -21.0), 1.0), trim, 3.0, 0.6 * zap);
        eyes(q, vec2(1.6, -11.5), eyeColor, rage);
        vec2 hand = pose == 1 || pose == 3 ? vec2(11.0, -6.0) : vec2(10.0, 3.0);
        part(sdCircle(q - hand, 3.0), body * 1.1);
        if (pose == 1 || pose == 3) {
            glow(sdCircle(q - hand, 3.0), trim, 6.0, 0.8);
        }
    } else if (kind == 2) {
        // RIMEFANG: a sleek skater with an ice crown and blade feet.
        float lean = pose == 2 ? 0.35 : 0.0;
        q = rot(q - vec2(0.0, 10.0), lean) + vec2(0.0, 10.0);
        part(sdSegment(q, vec2(-8.0, 13.0), vec2(4.0, 13.0), 1.0), trim); // the blade
        part(sdRoundBox(q - vec2(-2.0, 9.0), vec2(5.0, 3.0), 1.6), body * 0.8);
        part(sdRoundBox(q - vec2(0.0, -1.0), vec2(7.0, 8.0), 3.4), body);
        fill(sdRoundBox(q - vec2(2.0, -1.0), vec2(2.0, 6.0), 1.0), trim);
        part(sdCircle(q - vec2(0.0, -12.0), 5.6), body * 1.1);
        for (int i = 0; i < 3; ++i) {
            float x = -4.0 + 4.0 * float(i);
            float crown = sdSegment(q, vec2(x, -16.0), vec2(x * 1.3, -22.0 - 2.0 * float(i == 1)), 1.1);
            part(crown, trim);
        }
        eyes(q, vec2(2.0, -12.4), mix(vec3(0.6, 0.95, 1.0), vec3(1.0, 0.3, 0.3), rage), rage);
        vec2 arm = pose == 3 ? vec2(9.0, -6.0) : vec2(7.0, 1.0);
        part(sdSegment(q, vec2(4.0, -4.0), arm, 2.0), body * 0.95);
        if (pose == 3) {
            part(sdSegment(q, arm, arm + vec2(6.0, -1.0), 1.0), trim); // an ice shard in hand
        }
    } else if (kind == 3) {
        // THORNWOOD: a bark-armored giant crowned with leaves.
        part(sdRoundBox(q - vec2(-6.0, 13.0), vec2(5.0, 3.0), 1.2), body * 0.6);
        part(sdRoundBox(q - vec2(6.0, 13.0), vec2(5.0, 3.0), 1.2), body * 0.6);
        part(sdRoundBox(q - vec2(0.0, 2.0), vec2(13.0, 11.0), 5.0), vec3(0.45, 0.3, 0.18));
        for (int i = 0; i < 4; ++i) {
            fill(sdSegment(q, vec2(-9.0 + 6.0 * float(i), -6.0), vec2(-8.0 + 6.0 * float(i), 10.0), 0.6), vec3(0.3, 0.2, 0.12));
        }
        float leaves = min(min(sdCircle(q - vec2(-7.0, -13.0), 7.0), sdCircle(q - vec2(3.0, -16.0), 8.0)), sdCircle(q - vec2(10.0, -11.0), 6.0));
        part(leaves, body);
        fill(sdCircle(q - vec2(-2.0, -18.0), 2.0), trim);
        fill(sdCircle(q - vec2(7.0, -15.0), 1.6), trim);
        eyes(q, vec2(4.0, -3.0), eyeColor, rage);
        vec2 arm = pose == 1 || pose == 3 ? vec2(13.0, -9.0) : vec2(14.0, 3.0);
        part(sdSegment(q, vec2(9.0, -2.0), arm, 3.0), vec3(0.45, 0.3, 0.18));
        part(sdCircle(q - arm, 3.2), body);
    } else if (kind == 4) {
        // CINDERMAW: a round furnace with a glowing grill of a mouth.
        part(sdRoundBox(q - vec2(-6.0, 12.0), vec2(4.0, 3.5), 1.4), body * 1.4);
        part(sdRoundBox(q - vec2(6.0, 12.0), vec2(4.0, 3.5), 1.4), body * 1.4);
        part(sdCircle(q - vec2(0.0, 0.0), 12.5), body);
        float heat = 0.7 + 0.3 * sin(t * 5.0) + 0.4 * rage;
        float mouth = sdRoundBox(q - vec2(4.0, 3.5), vec2(6.0, pose == 1 || pose == 3 ? 3.4 : 2.0), 1.2);
        glow(mouth, trim, 5.0, 0.6 * heat);
        fill(mouth, trim * heat);
        for (int i = -2; i <= 2; ++i) {
            fill(sdBox(q - vec2(4.0 + 2.2 * float(i), 3.5), vec2(0.35, 3.4)), kInk);
        }
        // Chimney stacks breathing sparks.
        part(sdRoundBox(q - vec2(-6.0, -13.0), vec2(2.2, 3.5), 0.8), body * 1.3);
        part(sdRoundBox(q - vec2(3.0, -14.0), vec2(2.2, 4.0), 0.8), body * 1.3);
        glow(sdCircle(q - vec2(3.0, -19.0 - fract(t * 2.0) * 4.0), 1.0), trim, 3.0, 0.8 * (1.0 - fract(t * 2.0)));
        eyes(q, vec2(4.0, -5.0), eyeColor, rage);
    } else if (kind == 5) {
        // GALEWING: a bird-like flier with broad wings and a beaked helmet.
        float flap = sin(t * (pose == 2 ? 14.0 : 6.0)) * 0.5;
        vec2 w = rot(q - vec2(-2.0, -2.0), -0.2 + flap);
        float wing = sdEllipse(w - vec2(-10.0, -2.0), vec2(12.0, 3.4));
        part(wing, body);
        for (int i = 0; i < 3; ++i) {
            fill(sdSegment(w, vec2(-6.0 - 5.0 * float(i), -1.0), vec2(-9.0 - 5.0 * float(i), 2.5), 0.6), trim);
        }
        part(sdEllipse(q - vec2(0.0, 1.0), vec2(9.0, 7.0)), body);
        fill(sdEllipse(q - vec2(1.5, 3.0), vec2(5.0, 3.5)), trim);
        part(sdCircle(q - vec2(5.0, -6.0), 5.4), body);
        part(sdSegment(q, vec2(8.0, -5.0), vec2(14.0, -3.0), 1.4), vec3(1.0, 0.8, 0.3)); // the beak
        eyes(q, vec2(6.0, -7.0), eyeColor, rage);
        part(sdSegment(q, vec2(-2.0, 7.0), vec2(-2.0, 11.0), 1.0), vec3(1.0, 0.8, 0.3));
        part(sdSegment(q, vec2(2.0, 7.0), vec2(2.0, 11.0), 1.0), vec3(1.0, 0.8, 0.3));
    } else {
        // THE OVERSEER: a wall-mounted machine skull around a great red eye.
        q = vec2(p.x, p.y); // always faces the room
        for (int i = 0; i < 3; ++i) {
            float y = -18.0 + 18.0 * float(i);
            part(sdRoundBox(q - vec2(24.0, y), vec2(8.0, 3.0), 1.4), body * 0.7); // pipes into the wall
        }
        part(sdRoundBox(q - vec2(2.0, 0.0), vec2(22.0, 28.0), 10.0), body);
        part(sdRoundBox(q - vec2(-2.0, 20.0), vec2(14.0, 6.0), 3.0), body * 0.8); // the jaw
        for (int i = -3; i <= 3; ++i) {
            fill(sdBox(q - vec2(-2.0 + 3.6 * float(i), 18.0), vec2(1.2, 2.6)), vec3(0.85));
        }
        float open = pose == 1 || pose == 3 ? 1.0 : 0.6 + 0.1 * sin(t * 2.0);
        float socket = sdEllipse(q - vec2(-6.0, -4.0), vec2(11.0, 9.0 * open + 0.5));
        part(socket, kInk * 2.0);
        vec2 look = vec2(-2.0 - 2.0 * sin(t * 0.7), 0.0);
        float iris = sdCircle(q - vec2(-6.0, -4.0) - look, 5.5);
        glow(iris, trim, 8.0, 0.6 + 0.3 * rage);
        fill(max(iris, socket), trim * (1.0 + 0.4 * rage));
        fill(max(sdCircle(q - vec2(-6.0, -4.0) - look, 2.0), socket), vec3(1.0, 0.9, 0.8));
        fill(sdBox(q - vec2(2.0, -22.0), vec2(14.0, 1.2)), trim * 0.7);
    }

    vec3 rgb = acc.rgb;
    if (flash) {
        rgb = mix(rgb, vec3(1.0), 0.8);
    }
    outColor = vec4(rgb, acc.a * s.color.a);
}
