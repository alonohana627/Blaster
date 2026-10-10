#version 450
#include "gamelib/sprite.glsl"
#include "gamelib/sdf.glsl"
#include "gamelib/noise.glsl"

// Every tile kind of every stage (draw::drawTiles): ground and walls with a
// look per theme, blocks, one-way platforms, ladders, spikes, lava, ice,
// conveyor belts, mini-boss shutters and lift rails. One tile is one 16x16
// quad in world units; its neighbours decide which edges get a rim.
//   color   fill      color2   rim / cap / detail
//   params  x kind (0 ground, 1 block, 2 one-way, 3 ladder, 4 spikes, 5 lava,
//           6 ice, 7 conveyor left, 8 conveyor right, 9 shutter, 10 rail
//           across, 11 rail up, 12 boss door), y theme (0..7), z the door's
//           lower edge (world y), w -
//   bits    neighbour mask | variant << 8 | cell x << 32 | cell y << 48

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

const uint N = 1u, E = 2u, S = 4u, W = 8u;

float hash(ivec2 c, uint salt)
{
    return hash12(uvec2(c + ivec2(4096)) * uvec2(1u, 1u) + uvec2(salt, salt * 7u));
}

void main()
{
    SpriteData s = sprite;
    vec2 p = spriteLocal(inUv);          // -8..8, y down
    vec2 uv = p / 16.0 + 0.5;            // 0..1 inside the tile
    int kind = int(s.params.x + 0.5);
    int theme = int(s.params.y + 0.5);
    uint mask = s.bits.x & 0xffu;
    ivec2 cell = ivec2(int(s.bits.y & 0xffffu), int(s.bits.y >> 16));
    vec2 world = vec2(cell) * 16.0 + uv * 16.0;
    float time = mod(s.time, 3600.0);
    vec3 fill = s.color.rgb;
    vec3 rim = s.color2.rgb;
    vec4 c = vec4(0.0);

    if (kind == 0 || kind == 1 || kind == 6) {
        bool open[4] = bool[4]((mask & N) == 0u, (mask & E) == 0u, (mask & S) == 0u, (mask & W) == 0u);
        vec3 col = fill;
        float n = valueNoise(world / 5.0);
        if (kind == 1) {
            // A block: bevelled, with a cross brace.
            float edge = max(abs(p.x), abs(p.y));
            col = mix(fill, rim, smoothstep(6.0, 7.5, edge) * 0.8);
            col = mix(col, fill * 0.7, coverage(min(abs(p.x - p.y), abs(p.x + p.y)) - 0.8) * step(edge, 5.5));
        } else if (kind == 6) {
            // Ice: pale with a diagonal shine.
            col = mix(fill, vec3(1.0), smoothstep(0.92, 1.0, sin((world.x + world.y) / 9.0)) * 0.5);
        } else if (theme == 0 || theme == 7) {
            // Metal panels with rivets.
            vec2 f = fract(world / 32.0) * 32.0;
            float seam = min(min(f.x, 32.0 - f.x), min(f.y, 32.0 - f.y));
            col = fill * (0.92 + 0.08 * n);
            col = mix(col, fill * 0.6, smoothstep(1.2, 0.4, seam));
            float rivet = length(f - vec2(4.0)) - 1.2;
            col = mix(col, rim * 0.8, coverage(rivet) * 0.7);
            if (theme == 7) {
                float lamp = step(0.93, hash(ivec2(floor(world / 32.0)), 5u));
                col = mix(col, rim, lamp * coverage(length(f - vec2(16.0)) - 2.5) * (0.6 + 0.4 * sin(time * 3.0)));
            }
        } else if (theme == 1) {
            // Rusty plates with bolts.
            vec2 f = fract(world / vec2(32.0, 16.0)) * vec2(32.0, 16.0);
            float seam = min(f.y, min(f.x, 32.0 - f.x));
            col = mix(fill, fill * vec3(1.15, 0.85, 0.7), smoothstep(0.55, 0.8, fbm(world / 9.0, 2)));
            col = mix(col, fill * 0.55, smoothstep(1.0, 0.3, seam));
            col = mix(col, rim, coverage(length(f - vec2(3.0, 8.0)) - 1.1) * 0.6);
        } else if (theme == 2 || theme == 5) {
            // Stone bricks; glowing cracks in the magma works.
            float row = floor(world.y / 8.0);
            vec2 f = vec2(fract((world.x + 6.0 * mod(row, 2.0)) / 14.0) * 14.0, fract(world.y / 8.0) * 8.0);
            float seam = min(min(f.x, 14.0 - f.x), min(f.y, 8.0 - f.y));
            col = fill * (0.85 + 0.2 * n);
            col = mix(col, fill * 0.5, smoothstep(1.0, 0.3, seam));
            if (theme == 5) {
                float crack = abs(valueNoise(world / 6.0) - 0.5);
                col = mix(col, rim * (0.9 + 0.3 * sin(time * 2.0 + world.x / 20.0)), smoothstep(0.04, 0.0, crack) * 0.8);
            }
        } else if (theme == 3) {
            // Packed snow and ice.
            col = mix(fill, rim, 0.15 + 0.2 * n);
            col = mix(col, vec3(1.0), smoothstep(0.96, 1.0, sin((world.x - world.y) / 7.0)) * 0.3);
        } else if (theme == 4) {
            // Earth with roots and stones.
            col = fill * (0.85 + 0.25 * n);
            float stone = length(fract(world / 11.0) - 0.5) - 0.18;
            col = mix(col, fill * 1.4, coverage(stone * 11.0) * step(0.7, hash(ivec2(floor(world / 11.0)), 3u)));
        } else {
            // Sky marble with a soft vein.
            col = mix(fill, rim, smoothstep(0.48, 0.52, valueNoise(world / 14.0)) * 0.2);
        }
        // Rims on the open sides; a thicker cap on top.
        float capW = kind == 0 && (theme == 4 || theme == 3) ? 4.5 : 2.2;
        float d = 1e3;
        if (open[0]) d = min(d, (p.y + 8.0) / capW);
        if (open[1]) d = min(d, (8.0 - p.x) / 1.6);
        if (open[2]) d = min(d, (8.0 - p.y) / 1.6);
        if (open[3]) d = min(d, (p.x + 8.0) / 1.6);
        float edge = 1.0 - smoothstep(0.85, 1.0, d);
        vec3 edgeColor = open[0] && (p.y + 8.0) / capW < 1.0 ? rim : fill * 0.55;
        if (kind == 0 && theme == 4 && open[0]) {
            // Grass blades on top.
            float blade = sin(world.x * 1.7) * 0.5 + 0.5;
            edge = max(edge, step(p.y + 8.0, 3.0 + 2.5 * blade));
            edgeColor = rim;
        }
        col = mix(col, edgeColor, edge);
        c = vec4(col, 1.0);
    } else if (kind == 2) {
        // A one-way platform: a slab on top with brackets under it.
        float slab = sdRoundBox(p - vec2(0.0, -5.5), vec2(8.2, 2.5), 0.8);
        c = over(vec4(rim, coverage(slab)), c);
        c = over(vec4(fill, coverage(sdBox(p - vec2(0.0, -4.5), vec2(8.2, 1.0)))), c);
        float bracket = sdSegment(p, vec2(-5.0, -3.0), vec2(-1.0, 3.0), 0.9);
        bracket = min(bracket, sdSegment(p, vec2(5.0, -3.0), vec2(1.0, 3.0), 0.9));
        c = over(vec4(fill * 0.6, coverage(bracket) * 0.85), c);
    } else if (kind == 3) {
        // A ladder: two rails and rungs.
        float rails = min(abs(p.x - 5.5), abs(p.x + 5.5)) - 1.1;
        float rungs = max(abs(fract(world.y / 5.333) - 0.5) * 5.333 - 0.8, abs(p.x) - 5.5);
        c = over(vec4(rim, coverage(rails)), c);
        c = over(vec4(fill, coverage(rungs)), c);
    } else if (kind == 4) {
        // Spikes: a row of steel teeth.
        float x = (fract(uv.x * 2.0) - 0.5) * 8.0;
        float spike = (abs(x) - 4.0 * (p.y + 6.0) / 14.0) * 0.96;
        c = over(vec4(mix(fill, rim, step(0.0, x)), coverage(spike)), c);
        c = over(vec4(fill * 0.5, coverage(sdBox(p - vec2(0.0, 6.5), vec2(8.0, 1.5)))), c);
    } else if (kind == 5) {
        // Lava (or acid): glowing, flowing, a bright surface.
        float flow = fbm(vec2(world.x / 14.0 + time * 0.4, world.y / 10.0 - time * 0.2), 3);
        vec3 col = mix(fill, rim, flow);
        bool top = (mask & N) == 0u;
        if (top) {
            float wave = -6.0 + 1.2 * sin(world.x / 6.0 + time * 3.0);
            col = mix(col, rim * 1.4, smoothstep(wave + 2.0, wave, p.y));
            c = vec4(col * 1.3, coverage(-(p.y - wave)));
        } else {
            c = vec4(col * 1.2, 1.0);
        }
    } else if (kind == 7 || kind == 8) {
        // A conveyor belt: chevrons moving its way.
        float dir = kind == 7 ? -1.0 : 1.0;
        float body = sdRoundBox(p, vec2(8.0, 7.0), 2.0);
        c = over(vec4(fill * 0.7, coverage(body)), c);
        float belt = sdBox(p - vec2(0.0, -5.5), vec2(8.0, 2.5));
        float chevron = abs(fract((world.x - dir * time * 42.0) / 8.0 + abs(p.y + 5.5) / 16.0 * dir) - 0.5) - 0.18;
        c = over(vec4(mix(rim, fill * 0.4, step(0.0, chevron)), coverage(belt)), c);
        float wheel = length(vec2(fract(world.x / 8.0) * 8.0 - 4.0, p.y - 2.5)) - 2.2;
        c = over(vec4(rim * 0.8, coverage(wheel)), c);
    } else if (kind == 9) {
        // A shutter: warning stripes on a heavy plate.
        float stripe = step(0.5, fract((world.x + world.y) / 10.0));
        vec3 col = mix(vec3(0.95, 0.75, 0.1), vec3(0.08), stripe);
        col = mix(col, vec3(0.08), smoothstep(6.0, 7.5, abs(p.x)));
        c = vec4(col, 1.0);
    } else if (kind == 12) {
        // A boss door: heavy slats that slide up into the frame as it opens;
        // params.z is the world y of its lower edge now.
        float cut = s.params.z - world.y;
        float slat = abs(fract(world.y / 4.0) - 0.5) * 4.0;
        vec3 col = mix(fill, fill * 0.55, smoothstep(1.4, 1.9, slat));
        col = mix(col, rim, smoothstep(5.0, 7.0, abs(p.x)));
        float light = length(vec2(p.x, fract(world.y / 16.0) * 16.0 - 8.0)) - 1.6;
        col = mix(col, rim * 1.4, coverage(light) * (0.6 + 0.4 * sin(time * 5.0)));
        c = vec4(col, clamp(cut, 0.0, 1.0));
    } else if (kind == 10 || kind == 11) {
        // A lift's rail: a dotted line.
        vec2 q = kind == 10 ? p : p.yx;
        float dot_ = length(vec2(fract(q.x / 6.0) * 6.0 - 3.0, q.y)) - 1.0;
        c = vec4(fill, coverage(dot_) * 0.6);
    }
    outColor = vec4(c.rgb, c.a * s.color.a);
}
