#include "FastNoiseLite.glsl"

vec3 skyColor(vec3 dir, vec3 pos, float time)
{
    dir = normalize(dir);

    // sky gradient
    float h = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);

    vec3 horizon = vec3(0.9,0.65,0.45);
    vec3 skyMid  = vec3(0.35,0.55,0.95);
    vec3 skyTop  = vec3(0.15,0.35,0.85);

    vec3 sky = mix(horizon, skyMid, smoothstep(0.0,0.4,h));
    sky = mix(sky, skyTop, pow(h,1.5));

    if(dir.y <= 0.0)
        return sky;

    // cloud plane intersection
    float cloudHeight = 1000.0;
    float t = (cloudHeight - pos.y) / dir.y;

    vec3 worldHit = pos + dir * t;

    vec2 uv = worldHit.xz * 0.0004;

    // wind motion (domain translation)
    uv += vec2(30.0,15.0) * time * 0.0004;

    // FastNoiseLite state
    fnl_state noise;
    noise.seed = 1337;
    noise.frequency = 1.0;
    noise.noise_type = FNL_NOISE_OPENSIMPLEX2;
    noise.fractal_type = FNL_FRACTAL_FBM;
    noise.octaves = 5;
    noise.lacunarity = 2.0;
    noise.gain = 0.5;

    float n1 = fnlGetNoise2D(noise, uv.x, uv.y);
    float n2 = fnlGetNoise2D(noise, uv.x*3.0, uv.y*3.0) * 0.4;
    float n = n1 + n2*sin(time);

    n = n * 0.5 + 0.5;

    float clouds = smoothstep(0.55, 0.75, n);

    vec3 cloudColor = vec3(1.0);

    return mix(sky, cloudColor, clouds * 0.8);
}