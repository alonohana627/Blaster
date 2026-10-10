#version 450
#include "gamelib/sprite.glsl"
#include "gamelib/sdf.glsl"
#include "gamelib/noise.glsl"

// Every stage's backdrop, one full-canvas parallax quad per layer
// (draw::drawParallax): the sky, a far silhouette, a near silhouette and an
// animated weather layer drawn in front of the tiles. Each theme picks its
// own heightfields and extras from the same kit.
//   color   the layer's main color   color2   its light / detail color
//   params  x, y scroll (canvas units), z theme (0 lab, 1 factory, 2 storm,
//           3 ice, 4 forest, 5 magma, 6 sky, 7 fortress), w layer (0 sky,
//           1 far, 2 near, 3 weather)

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

const float kPi = 3.14159265;

float hash1(float x)
{
    return hash12(uvec2(uint(int(floor(x)) + 100000), 7u));
}

float hash2(vec2 c)
{
    return hash12(uvec2(ivec2(floor(c)) + ivec2(100000)));
}

// A soft edge for a heightfield: 1 below the surface (y down), anti-aliased.
float below(float y, float h)
{
    float w = max(fwidth(y), 1.0);
    return clamp((y - h) / w + 0.5, 0.0, 1.0);
}

// --- the sky --------------------------------------------------------------

vec4 sky(vec2 px, vec2 world, int theme, float time, vec3 top, vec3 low)
{
    float v = clamp(px.y / sprite.size.y, 0.0, 1.0);
    vec3 c = mix(top, low, smoothstep(0.0, 1.0, v));
    if (theme == 0 || theme == 3 || theme == 7) {
        // Stars: one per cell, inside the cell's middle.
        vec2 cell = floor(world / 46.0);
        vec2 f = fract(world / 46.0) - 0.5;
        float h = hash2(cell);
        vec2 o = vec2(hash2(cell + 17.0), hash2(cell + 31.0)) * 0.5 - 0.25;
        float star = smoothstep(0.08, 0.0, length(f - o)) * step(0.72, h) * (1.0 - v);
        star *= 0.6 + 0.4 * sin(time * (1.0 + h * 3.0) + h * 20.0);
        c += vec3(star) * 0.8;
    }
    if (theme == 0) {
        // A pale moon.
        vec2 m = px - vec2(sprite.size.x * 0.78, sprite.size.y * 0.2);
        c = mix(c, vec3(0.95, 0.92, 0.8), coverage(length(m) - 46.0));
        c += vec3(0.3, 0.32, 0.4) * glowFalloff(length(m) - 46.0, 140.0) * 0.4;
    }
    if (theme == 1 || theme == 7) {
        // Smoke or haze drifting.
        float n = fbm(vec2(world.x / 420.0 + time * 0.02, world.y / 160.0), 3);
        c = mix(c, low * 0.8 + 0.05, smoothstep(0.45, 0.8, n) * 0.35);
    }
    if (theme == 2) {
        // Storm clouds and a far flash every few seconds.
        float n = fbm(vec2(world.x / 300.0 + time * 0.05, world.y / 120.0), 3);
        c = mix(c, low * 0.55, smoothstep(0.35, 0.75, n) * 0.6);
        float flash = pow(max(0.0, sin(time * 0.9) * sin(time * 2.3 + 1.0)), 18.0);
        c += vec3(0.5, 0.5, 0.7) * flash * (1.0 - v);
    }
    if (theme == 3) {
        // Aurora ribbons.
        float band = sin(world.x / 260.0 + time * 0.3 + 2.0 * valueNoise(vec2(world.x / 300.0, time * 0.1)));
        float y = 0.22 + 0.06 * band;
        float a = exp(-pow((v - y) / 0.05, 2.0)) * (0.5 + 0.5 * valueNoise(vec2(world.x / 90.0 + time * 0.4, 0.0)));
        c += vec3(0.2, 0.9, 0.7) * a * 0.35;
    }
    if (theme == 4) {
        // Light shafts through the canopy.
        float s = sin((world.x + px.y * 0.6) / 70.0 + time * 0.2);
        c += vec3(0.6, 0.7, 0.3) * smoothstep(0.85, 1.0, s) * 0.12 * (1.0 - v);
    }
    if (theme == 5) {
        // The glow of the magma below.
        c += vec3(0.9, 0.25, 0.05) * smoothstep(0.5, 1.0, v) * (0.35 + 0.08 * sin(time * 1.7));
    }
    if (theme == 6) {
        // The sun and soft clouds.
        vec2 s = px - vec2(sprite.size.x * 0.2, sprite.size.y * 0.18);
        c = mix(c, vec3(1.0, 0.97, 0.85), coverage(length(s) - 54.0));
        c += vec3(1.0, 0.9, 0.6) * glowFalloff(length(s) - 54.0, 220.0) * 0.35;
        float n = fbm(vec2(world.x / 380.0 + time * 0.015, world.y / 140.0), 3);
        c = mix(c, vec3(1.0), smoothstep(0.55, 0.75, n) * 0.6);
    }
    if (theme == 7) {
        // Searchlights sweeping.
        for (int i = 0; i < 2; ++i) {
            float base = sprite.size.x * (0.3 + 0.4 * float(i));
            float angle = 0.5 * sin(time * (0.35 + 0.15 * float(i)) + float(i) * 2.0);
            vec2 d = px - vec2(base, sprite.size.y);
            float across = abs(dot(d, vec2(cos(angle), sin(angle))));
            c += vec3(0.9, 0.2, 0.25) * smoothstep(60.0, 0.0, across) * 0.12 * v;
        }
    }
    return vec4(c, 1.0);
}

// --- far silhouettes ------------------------------------------------------

vec4 far(vec2 px, vec2 world, int theme, float time, vec3 col, vec3 light)
{
    float horizon = sprite.size.y * 0.62 - sprite.params.y;
    float x = world.x;
    float h = horizon;
    vec3 detail = vec3(0.0);
    float detailA = 0.0;
    if (theme == 0 || theme == 7) {
        // A skyline: flat roofs per cell, lit windows.
        float cw = theme == 7 ? 150.0 : 110.0;
        float cell = floor(x / cw);
        float r = hash1(cell);
        h = horizon - 80.0 - r * (theme == 7 ? 320.0 : 220.0);
        float fx = fract(x / cw);
        if (fx < 0.08 || fx > 0.92) {
            h = horizon - 40.0 - r * 60.0;
        }
        if (theme == 7 && fx > 0.42 && fx < 0.58) {
            h -= 70.0; // a spire
        }
        vec2 win = vec2(fract(x / 18.0), fract(world.y / 26.0));
        float on = step(0.55, hash2(vec2(floor(x / 18.0), floor(world.y / 26.0))));
        detailA = on * step(0.3, win.x) * step(win.x, 0.7) * step(0.3, win.y) * step(win.y, 0.7) * step(h + 20.0, world.y);
        detail = light;
        if (theme == 7) {
            float blink = step(0.5, fract(time * 0.7 + r));
            detailA = max(detailA * 0.4, blink * smoothstep(6.0, 0.0, length(vec2(fract(x / cw) * cw - cw * 0.5, world.y - h - 8.0))));
        }
    } else if (theme == 1) {
        // Smokestacks over factory roofs.
        float cell = floor(x / 140.0);
        float r = hash1(cell);
        float fx = fract(x / 140.0);
        h = horizon - 90.0 - 40.0 * step(0.5, fract(x / 60.0)); // saw-tooth roofs
        h = horizon - 70.0 - 50.0 * fract(x / 70.0);
        if (abs(fx - 0.5) < 0.09) {
            h = horizon - 200.0 - r * 160.0;
        }
        float smoke = fbm(vec2(x / 60.0, world.y / 50.0 + time * 0.5), 2);
        float top = horizon - 200.0 - r * 160.0;
        detailA = smoothstep(0.5, 0.8, smoke) * smoothstep(40.0, 0.0, abs(fx - 0.5) * 140.0 - (top - world.y) * 0.25)
                  * step(world.y, top) * smoothstep(top - 260.0, top, world.y) * 0.5;
        detail = light * 0.5 + 0.2;
    } else if (theme == 2 || theme == 3 || theme == 5) {
        // Ridged peaks.
        float n = 0.0;
        float amp = 1.0;
        float f = 1.0 / 260.0;
        for (int i = 0; i < 3; ++i) {
            n += amp * (1.0 - abs(valueNoise(vec2(x * f, 3.0 + float(i))) * 2.0 - 1.0));
            amp *= 0.5;
            f *= 2.3;
        }
        h = horizon - 60.0 - n * (theme == 5 ? 150.0 : 190.0);
        if (theme == 3) {
            detailA = step(world.y, h + 40.0 + 25.0 * valueNoise(vec2(x / 40.0, 1.0)));
            detail = light;
        } else if (theme == 5) {
            float vein = valueNoise(vec2(x / 30.0, world.y / 30.0));
            detailA = smoothstep(0.75, 0.8, vein) * (0.6 + 0.4 * sin(time * 2.0 + x / 50.0));
            detail = light;
        } else {
            detailA = 0.0;
        }
    } else if (theme == 4) {
        // A canopy of round crowns.
        float cell = floor(x / 90.0);
        h = 1e6;
        for (int i = -1; i <= 1; ++i) {
            float c = cell + float(i);
            float r = 50.0 + hash1(c) * 50.0;
            vec2 center = vec2((c + 0.5) * 90.0, horizon - 120.0 - hash1(c + 9.0) * 120.0);
            float dx = x - center.x;
            if (abs(dx) < r) {
                h = min(h, center.y - sqrt(r * r - dx * dx));
            }
        }
        h = min(h, horizon - 60.0);
        detailA = smoothstep(0.6, 0.9, valueNoise(world / 22.0)) * 0.35;
        detail = light;
    } else if (theme == 6) {
        // Cloud banks.
        float n = fbm(vec2(x / 220.0 + time * 0.01, 2.0), 3);
        h = horizon + 60.0 - n * 220.0;
        detailA = smoothstep(h + 30.0, h, world.y) * 0.6;
        detail = light;
    }
    float inside = below(world.y, h);
    vec3 c = mix(col, detail, detailA);
    return vec4(c, inside);
}

// --- near silhouettes -----------------------------------------------------

vec4 near(vec2 px, vec2 world, int theme, float time, vec3 col, vec3 light)
{
    float ground = sprite.size.y * 0.8 - sprite.params.y;
    float x = world.x;
    float a = 0.0;
    vec3 c = col;
    if (theme == 0 || theme == 7) {
        // Towers and antennas with blinking lamps.
        float cw = 260.0;
        float cell = floor(x / cw);
        float r = hash1(cell + 4.0);
        float fx = (fract(x / cw) - 0.5) * cw;
        float top = ground - 220.0 - r * 260.0;
        float tower = max(abs(fx) - 26.0 - 10.0 * step(top + 60.0, world.y), top - world.y);
        float mast = max(abs(fx) - 3.0, top - 90.0 - world.y);
        a = coverage(min(tower, mast));
        float lamp = length(vec2(fx, world.y - (top - 90.0)));
        float blink = step(0.5, fract(time * 0.8 + r));
        c = mix(c, light, coverage(lamp - 5.0) * blink);
        a = max(a, coverage(lamp - 5.0) * blink);
    } else if (theme == 1) {
        // Girders: X trusses between posts.
        float top = ground - 330.0;
        float fx = fract(x / 160.0);
        float fy = fract((world.y - top) / 160.0);
        float post = step(abs(fx - 0.5), 0.06);
        float beam = step(abs(fy - 0.0), 0.05) + step(0.95, fy);
        float diag = step(abs(fx - fy), 0.035) + step(abs(fx - (1.0 - fy)), 0.035);
        a = clamp(post + beam + diag, 0.0, 1.0) * step(top, world.y);
        // A turning gear now and then.
        vec2 g = vec2(mod(x, 640.0) - 320.0, world.y - (top + 90.0));
        float ang = atan(g.y, g.x) + time * 0.5;
        float gear = length(g) - 58.0 - 8.0 * step(0.0, sin(ang * 10.0));
        a = max(a, coverage(gear) * (1.0 - coverage(length(g) - 18.0)));
    } else if (theme == 2) {
        // Radio towers with blinking red lamps.
        float cw = 300.0;
        float fx = (fract(x / cw) - 0.5) * cw;
        float r = hash1(floor(x / cw));
        float top = ground - 280.0 - r * 120.0;
        float width = mix(4.0, 40.0, clamp((world.y - top) / (ground - top), 0.0, 1.0));
        a = coverage(max(abs(fx) - width, top - world.y));
        float lamp = length(vec2(fx, world.y - top));
        float blink = step(0.6, fract(time * 1.1 + r));
        c = mix(c, light, coverage(lamp - 6.0) * blink);
        a = max(a, coverage(lamp - 6.0) * blink);
    } else if (theme == 3) {
        // Crystal spikes.
        float cw = 120.0;
        float cell = floor(x / cw);
        float fx = (fract(x / cw) - 0.5) * cw;
        float r = hash1(cell);
        float height = 120.0 + r * 200.0;
        float h = ground - height + abs(fx) * (height / 50.0);
        a = below(world.y, h) * step(abs(fx), 50.0);
        c = mix(col, light, 0.25 * step(0.0, fx));
    } else if (theme == 4) {
        // Tree trunks and hanging vines.
        float cw = 180.0;
        float fx = (fract(x / cw) - 0.5) * cw;
        float r = hash1(floor(x / cw));
        float trunk = abs(fx + 10.0 * sin(world.y / 80.0)) - 16.0 - r * 10.0;
        a = coverage(trunk);
        float vine = abs(fract(x / 47.0) - 0.5) * 47.0 - 1.5;
        float vineLen = 140.0 + 120.0 * hash1(floor(x / 47.0) + 3.0);
        a = max(a, coverage(vine) * step(world.y, -sprite.params.y + vineLen) * 0.8);
    } else if (theme == 5) {
        // Rock pillars and falls of lava.
        float cw = 220.0;
        float fx = (fract(x / cw) - 0.5) * cw;
        float r = hash1(floor(x / cw));
        float top = ground - 160.0 - r * 200.0;
        a = coverage(max(abs(fx) - 40.0 - 8.0 * valueNoise(vec2(world.y / 30.0, r * 9.0)), top - world.y));
        float fall = abs(fx - 50.0) - 6.0;
        float flow = valueNoise(vec2(fx / 6.0, world.y / 25.0 - time * 3.0));
        float lava = coverage(fall) * step(top + 20.0, world.y) * step(0.45, r);
        c = mix(c, light * (0.8 + 0.6 * flow), lava);
        a = max(a, lava);
    } else if (theme == 6) {
        // Floating islands of rock.
        float cw = 420.0;
        float cell = floor(x / cw);
        float r = hash1(cell);
        vec2 center = vec2((cell + 0.5) * cw, ground - 200.0 - r * 120.0 + 10.0 * sin(time * 0.6 + r * 6.0));
        vec2 d = vec2(x, world.y) - center;
        float island = max(sdEllipse(d, vec2(110.0, 30.0)), -d.y - 4.0);
        float rock = max(max(abs(d.x) - (95.0 - d.y * 1.1), -d.y), d.y - 85.0);
        a = coverage(min(island, rock));
        c = mix(col, light, coverage(max(island, -(d.y + 2.0))) * 0.5);
    }
    return vec4(c, a);
}

// --- weather in front of the tiles ----------------------------------------

vec4 weather(vec2 px, vec2 world, int theme, float time, vec3 col)
{
    float size = 64.0;
    vec4 acc = vec4(0.0);
    for (int layer = 0; layer < 2; ++layer) {
        float scale = 1.0 + float(layer) * 0.7;
        float speed = 1.0 / scale;
        vec2 p = world * scale;
        if (theme == 2) {
            // Slanted rain streaks.
            vec2 q = vec2(p.x + p.y * 0.25 + time * 120.0, p.y - time * 900.0 * speed);
            vec2 cell = floor(q / vec2(24.0, 90.0));
            vec2 f = fract(q / vec2(24.0, 90.0)) - 0.5;
            float h = hash2(cell + float(layer) * 37.0);
            float d = sdSegment(f * vec2(24.0, 90.0), vec2(0.0, -18.0), vec2(0.0, 18.0), 1.0);
            acc = over(vec4(col, coverage(d) * step(0.5, h) * 0.35), acc);
        } else {
            vec2 drift = theme == 3 ? vec2(sin(time * 0.7) * 30.0, -time * 60.0)
                       : theme == 5 ? vec2(sin(time * 0.5) * 20.0, time * 70.0)
                       : theme == 4 ? vec2(-time * 40.0, -time * 35.0)
                       : theme == 1 ? vec2(-time * 15.0, time * 25.0)
                                    : vec2(-time * 10.0, time * 8.0);
            vec2 q = p + drift * speed;
            vec2 cell = floor(q / size);
            vec2 f = fract(q / size) - 0.5;
            float h = hash2(cell + float(layer) * 51.0);
            vec2 o = vec2(hash2(cell + 7.0), hash2(cell + 13.0)) * 0.5 - 0.25;
            float r = theme == 4 ? 3.2 : (theme == 5 ? 2.2 : 2.6);
            vec2 d = (f - o) * size;
            if (theme == 4) {
                float a = time * 2.0 + h * 6.0;
                d = mat2(cos(a), -sin(a), sin(a), cos(a)) * d;
                d.y *= 2.2; // a leaf
            }
            float shape = length(d) - r / scale;
            float on = step(theme == 0 || theme == 6 || theme == 7 ? 0.82 : 0.6, h);
            float twinkle = theme == 6 ? 0.5 + 0.5 * sin(time * 4.0 + h * 30.0) : 1.0;
            acc = over(vec4(col, coverage(shape) * on * twinkle * (theme == 5 ? 0.85 : 0.55)), acc);
        }
    }
    return acc;
}

void main()
{
    vec2 px = inUv * sprite.size;
    vec2 world = mod(px + sprite.params.xy, 76800.0); // wrapped: no precision stripes far along a stage
    int theme = int(sprite.params.z + 0.5);
    int layer = int(sprite.params.w + 0.5);
    float time = mod(sprite.time, 3600.0);
    vec3 col = sprite.color.rgb;
    vec3 light = sprite.color2.rgb;
    vec4 c = vec4(0.0);
    if (layer == 0) {
        c = sky(px, world, theme, time, col, light);
    } else if (layer == 1) {
        c = far(px, world, theme, time, col, light);
    } else if (layer == 2) {
        c = near(px, world, theme, time, col, light);
    } else {
        c = weather(px, world, theme, time, col);
    }
    outColor = vec4(c.rgb, c.a * sprite.color.a);
}
