#version 450
#include "gamelib/sprite.glsl"
#include "gamelib/sdf.glsl"
#include "gamelib/noise.glsl"

// A glowing orb with swirling plasma inside, animated by time.
//   color = main color, color2 = highlight and glow
//   params.x pulse (0..1, brightens), params.y glow width (canvas units)

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

void main()
{
    vec2 p = spriteLocal(inUv);
    float glow = sprite.params.y;
    float radius = min(sprite.size.x, sprite.size.y) * 0.5 - glow;
    float d = sdCircle(p, radius);

    // Two layers of value noise drifting in different directions.
    vec2 q = p / radius;
    float t = sprite.time;
    float n = 0.6 * valueNoise(q * 3.0 + vec2(t * 0.7, -t * 0.5)) + 0.4 * valueNoise(q * 7.0 - vec2(t, t * 0.3));
    float rim = smoothstep(0.35, 1.0, length(q));
    vec3 core = mix(sprite.color.rgb, sprite.color2.rgb, n) * (1.25 - 0.55 * rim) + sprite.params.x * 0.4;

    vec4 body = vec4(core, sprite.color.a * coverage(d));
    vec4 halo = vec4(sprite.color2.rgb, (0.55 + 0.4 * sprite.params.x) * glowFalloff(d, glow));
    outColor = over(body, halo);
}
