#include "raytrace.slangh"
#include "sky.slangh"
#include "facehashmap.slangh"

#ifndef GI_SAMPLES
#define GI_SAMPLES 1
#endif

#if GI_SAMPLES < 1
#error GI_SAMPLES must be at least 1
#endif

#define PROBE_THREADS 64
#define PROBE_GROUPS_PER_ROW 65535u

static const float RT_RAY_BIAS       = 0.01;
static const float RT_HISTORY_RATE   = 0.002;
static const float RT_CACHE_FEEDBACK = 0.8;

struct CameraTransform {
    int3 chunkPos;
    float3 localPos;
    float3x3 rotation;
    float time;
    int frame;
};

[[vk::binding(0, 0)]]
StructuredBuffer<CameraTransform> cameraTransform;

[[vk::binding(1, 0)]]
StructuredBuffer<ContreeHeader> contreeHeaders;

[[vk::binding(2, 0)]]
StructuredBuffer<ContreeData> contreeData;

[[vk::binding(3, 0)]]
StructuredBuffer<Chunk> chunks;

[[vk::binding(4, 0)]]
StructuredBuffer<ChunkPositionsHeader> chunkPositionsHeader;

[[vk::binding(5, 0)]]
StructuredBuffer<uint32_t> chunkPositions;

[[vk::binding(0, 1)]]
RWStructuredBuffer<FaceEntry> faceHashmap;

[[vk::binding(1, 1)]]
RWStructuredBuffer<uint> placedProbes;

uint RT_Hash(uint x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    return x ^ (x >> 16);
}

float RT_Random(inout uint state) {
    state = RT_Hash(state + 0x9e3779b9u);
    return float(state >> 8) * (1.0 / 16777216.0);
}

TraceResult RT_Trace(Ray ray) {
    return TraceWorld(ray, contreeHeaders, contreeData, chunks, chunkPositionsHeader, chunkPositions);
}

float3 RT_Normal(float3 n, float3 incoming) {
    float len2 = dot(n, n);
    return len2 > 0.0 ? n * rsqrt(len2) : -incoming;
}


float3 RT_FaceCenter(TraceResult hit, float3 normal) {
    return float3(hit.position) + float3(0.5) + normal * 0.5;
}

uint RT_FaceIndex(float3 normal) {
    return abs(normal.x) > 0.5
        ? (normal.x > 0.0 ? 0u : 1u)
        : abs(normal.y) > 0.5
        ? (normal.y > 0.0 ? 2u : 3u)
        : (normal.z > 0.0 ? 4u : 5u);
}
uint RT_FaceSeed(TraceResult hit, float3 normal, float time) {
    uint3 voxel = uint3(hit.position);
    uint face = RT_FaceIndex(normal);
    uint key = RT_Hash(voxel.x);
    key = RT_Hash(key ^ voxel.y);
    key = RT_Hash(key ^ voxel.z);
    return RT_Hash(key ^ RT_Hash(face) ^ RT_Hash(asuint(time)));
}

float3 RT_Hemisphere(float3 normal, inout uint rng) {
    float u = RT_Random(rng);
    float angle = 6.28318530718 * RT_Random(rng);
    float s;
    float c;
    sincos(angle, s, c);
    float3 axis =
        abs(normal.y) < 0.999
        ? float3(0, 1, 0)
        : float3(1, 0, 0);

    float3 tangent = normalize(cross(axis, normal));
    float3 bitangent = cross(normal, tangent);

    return sqrt(u) * (c * tangent + s * bitangent) + sqrt(1.0 - u) * normal;
}

float3 RT_Direct(float3 position, float3 normal, float3 sunDir) {
    float cosine = saturate(dot(normal, sunDir));
    if (cosine <= 0.0)
        return float3(0);
    Ray shadowRay;
    shadowRay.origin = position + normal * RT_RAY_BIAS;
    shadowRay.direction = sunDir;
    TraceResult shadow = RT_Trace(shadowRay);
    return shadow.hit ? float3(0) : float3(cosine);
}

float3 RT_CachedLighting(TraceResult hit, float3 normal, bool swap, int frame, out bool valid) {
    valid = false;

    if (frame <= 0)
        return float3(0);

    FaceKey key;
    key.position = hit.position;
    key.face = RT_FaceIndex(normal);
    key.swap = !swap;
    key.frame = uint(frame - 1);
    FaceReadResult found = ReadKey(key, faceHashmap);

    valid =
        found.slotIndex != INVALID_SLOT &&
        found.entry.frame == uint(frame - 1) &&
        all(isfinite(found.entry.lighting)) &&
        all(found.entry.lighting >= float3(0));

    return valid ? found.entry.lighting : float3(0);
}

float3 RT_Indirect(float3 position, float3 normal, float3 albedo, float3 sunDir, float time, bool swap, int frame, inout uint rng) {
    float3 radiance = float3(0);
    float3 throughput = albedo;

    [loop]
    for (int bounce = 0; bounce < 2; ++bounce) {
        if (max(throughput.x, max(throughput.y,throughput.z)) <= 0.0)
            break;
        
        Ray ray;
        ray.origin = position + normal * RT_RAY_BIAS;
        ray.direction = RT_Hemisphere(normal, rng);
        TraceResult hit = RT_Trace(ray);

        if (!hit.hit) {
            radiance += throughput * skyColor(ray.direction, ray.origin, time, sunDir);
            break;
        }

        normal = RT_Normal(hit.normal, ray.direction);
        position = RT_FaceCenter(hit, normal);

        throughput *= saturate(hit.voxel.color());

        bool cacheValid;

        float3 cached = RT_CachedLighting(hit, normal, swap, frame, cacheValid);
        cached = min(cached, float3(1.0));
        radiance += throughput * (cacheValid ? cached * RT_CACHE_FEEDBACK : RT_Direct(position, normal, sunDir));

        if (bounce == 1)
            break;
    }


    return radiance;
}

[shader("compute")]
[numthreads(PROBE_THREADS, 1, 1)]
void main(uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID) {
    uint groupIndex = groupID.x + groupID.y * PROBE_GROUPS_PER_ROW;
    uint placedIndex = groupIndex * uint(PROBE_THREADS) + groupThreadID.x;
    uint placedBufferSize;
    uint placedStride;
    placedProbes.GetDimensions(placedBufferSize, placedStride);

    if (placedBufferSize <= 1u)
        return;

    uint placedCount = min(placedProbes[0], placedBufferSize - 1u);

    if (placedIndex >= placedCount)
        return;

    uint absoluteSlot = placedProbes[placedIndex + 1u];

    if (absoluteSlot == INVALID_SLOT)
        return;

    CameraTransform camera = cameraTransform[0];
    bool swap = IsEven(camera.frame);
    FaceEntry currentEntry = faceHashmap[absoluteSlot];
    if (currentEntry.frame != uint(camera.frame))
        return;

    uint face = currentEntry.face;

    if (face >= 6u)
        return;

    int3 voxelPosition = currentEntry.position;
    float3 normal = faceNormals[face];
    float3 position = float3(voxelPosition) + float3(0.5) + normal * 0.5;

    FaceReadResult previous;
    previous.slotIndex = INVALID_SLOT;
    previous.entry = EmptyFaceEntry();

    bool hasHistory = false;
    if (camera.frame > 0) {
        FaceKey previousKey;
        previousKey.position = voxelPosition;
        previousKey.face = face;
        previousKey.swap = !swap;
        previousKey.frame = uint(camera.frame - 1);
        previous = ReadKey(previousKey, faceHashmap);
        hasHistory =
            previous.slotIndex != INVALID_SLOT &&
            previous.entry.frame == uint(camera.frame - 1) &&
            all(isfinite(previous.entry.lighting)) &&
            all(previous.entry.lighting >= float3(0));
    }

    Ray materialRay;
    materialRay.origin = position + normal * RT_RAY_BIAS;
    materialRay.direction = -normal;

    TraceResult material = RT_Trace(materialRay);
    if (!material.hit || any(material.position != voxelPosition)) {
        SetLighting(absoluteSlot, hasHistory ? previous.entry.lighting : float3(0), faceHashmap);
        return;
    }

    float3 albedo = saturate(material.voxel.color());

    float sunSin;
    float sunCos;
    sincos(camera.time * 0.01, sunSin, sunCos);
    float3 sunDir = float3(sunSin, 1.0, sunCos) * 0.70710678118;
    uint rng = RT_FaceSeed(material, normal, camera.time);
    float3 direct = albedo * RT_Direct(position, normal, sunDir);

    float3 indirect = float3(0);
    [loop]
    for (int i = 0; i < GI_SAMPLES;  ++i) {
        indirect += RT_Indirect(position, normal, albedo, sunDir, camera.time, swap, camera.frame, rng);
    }
    float3 color = direct + indirect / float(GI_SAMPLES);

    if (!all(isfinite(color))) {
        SetLighting(absoluteSlot, hasHistory ? previous.entry.lighting : float3(0), faceHashmap);
        return;
    }

    float3 accumulated = max(color, float3(0));

    if (hasHistory) {
        float3 previousColor = albedo * previous.entry.lighting;
        accumulated = lerp(previousColor, accumulated, RT_HISTORY_RATE);
    }

    accumulated = saturate(accumulated);
    float3 lighting = accumulated / max(albedo, float3(1.0e-8));
    SetLighting(absoluteSlot, lighting, faceHashmap);
}