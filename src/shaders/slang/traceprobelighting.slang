#include "raytrace.slangh"
#include "sky.slangh"
#include "lightprobe.slangh"

#define PROBE_LIGHTING_THREADS 64

#ifndef PROBE_LIGHTING_SAMPLES
#define PROBE_LIGHTING_SAMPLES 2
#endif

static const uint PL_DIFFUSE_BOUNCES = 2u;

static const float PL_HISTORY_RATE = 0.002;
static const float PL_RAY_BIAS = 0.01;
static const float3 PL_SUN_COLOR = float3(1.0);

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
RWStructuredBuffer<LightChunk> lightChunks;

[[vk::binding(1, 1)]]
RWStructuredBuffer<uint> activeProbes;

TraceResult PL_Trace(float3 origin, float3 direction) {
    Ray ray;
    ray.origin = origin;
    ray.direction = direction;

    return TraceWorld(
        ray,
        contreeHeaders,
        contreeData,
        chunks,
        chunkPositionsHeader,
        chunkPositions
    );
}

uint PL_Hash(uint value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

float PL_RadicalInverse(uint index, uint base) {
    float inverseBase = 1.0 / float(base);
    float factor = inverseBase;
    float value = 0.0;

    [loop]
    while (index > 0u) {
        uint next = index / base;
        uint digit = index - next * base;

        value += float(digit) * factor;
        factor *= inverseBase;
        index = next;
    }

    return min(value, 0.99999994);
}

float PL_Sample(uint sequenceIndex, uint base, uint probeSeed, uint dimension) {
    uint hash = PL_Hash(probeSeed ^ (0x9e3779b9u * (dimension + 1u)));
    float rotation = float(hash >> 8) * (1.0 / 16777216.0);
    return frac(PL_RadicalInverse(sequenceIndex, base) + rotation);
}

float3 PL_Sphere(float2 samplePosition) {
    float z = samplePosition.x * 2.0 - 1.0;
    float angle = samplePosition.y * 6.28318530718;
    float radius = sqrt(max(1.0 - z * z, 0.0));
    float s, c;
    sincos(angle, s, c);
    return float3(radius * c, z, radius * s);
}

float3 PL_Hemisphere(float3 normal, float2 samplePosition) {
    float angle = samplePosition.y * 6.28318530718;

    float s, c;
    sincos(angle, s, c);

    float3 axis = abs(normal.y) < 0.999
        ? float3(0, 1, 0)
        : float3(1, 0, 0);

    float3 tangent = normalize(cross(axis, normal));
    float3 bitangent = cross(normal, tangent);

    return sqrt(samplePosition.x) * (c * tangent + s * bitangent)
         + sqrt(max(1.0 - samplePosition.x, 0.0)) * normal;
}

float3 PL_Sun(float3 origin, float3 sunDirection) {
    TraceResult shadow = PL_Trace(origin, sunDirection);
    return shadow.hit ? float3(0) : PL_SUN_COLOR;
}

float3 PL_ProbeOrigin(int3 probeBase, uint2 emptyMask, uint lowEmptyCount, uint emptyCount, float voxelSample, float3 positionSample) {
    uint rank = min(
        uint(voxelSample * float(emptyCount)),
        emptyCount - 1u
    );

    uint word;
    uint bitOffset;

    if (rank < lowEmptyCount) {
        word = emptyMask.x;
        bitOffset = 0u;
    } else {
        rank -= lowEmptyCount;
        word = emptyMask.y;
        bitOffset = 32u;
    }

    [loop]
    while (rank > 0u) {
        word &= word - 1u;
        --rank;
    }

    uint index = bitOffset + uint(firstbitlow(word));

    int3 local = int3(
        int(index & 3u),
        int((index >> 2u) & 3u),
        int(index >> 4u)
    );

    float3 jitter = 0.1 + 0.8 * positionSample;

    return float3(probeBase + local) + jitter;
}

float3 PL_Indirect(float3 origin, float3 direction, float3 sunDirection, float time, uint sequenceIndex, uint probeSeed) {
    float3 radiance = float3(0);
    float3 throughput = float3(1);

    const uint bounceBases[4] = {
        17u, 19u,
        23u, 29u
    };

    [loop]
    for (uint bounce = 0u; bounce <= PL_DIFFUSE_BOUNCES; ++bounce) {
        TraceResult hit = PL_Trace(origin, direction);

        if (!hit.hit) {
            radiance += throughput * skyColor(direction, origin, time, sunDirection);
            break;
        }

        if (hit.depth <= PL_RAY_BIAS)
            break;

        float3 normal = float3(hit.normal);
        float lengthSquared = dot(normal, normal);

        normal = lengthSquared > 0.000001
            ? normal * rsqrt(lengthSquared)
            : -direction;

        float3 position = origin + direction * hit.depth;
        float3 bounceOrigin = position + normal * PL_RAY_BIAS;

        throughput *= saturate(hit.voxel.color());

        if (max(throughput.x, max(throughput.y, throughput.z)) <= 0.0)
            break;

        float cosine = saturate(dot(normal, sunDirection));

        if (cosine > 0.0) {
            radiance += throughput * cosine * PL_Sun(bounceOrigin, sunDirection);
        }

        if (bounce == PL_DIFFUSE_BOUNCES)
            break;

        float2 bounceSample = float2(
            PL_Sample(sequenceIndex, bounceBases[bounce * 2u], probeSeed, 6u + bounce * 2u),
            PL_Sample(sequenceIndex, bounceBases[bounce * 2u + 1u], probeSeed, 7u + bounce * 2u)
        );

        origin = bounceOrigin;
        direction = PL_Hemisphere(normal, bounceSample);
    }

    return radiance;
}

[shader("compute")]
[numthreads(PROBE_LIGHTING_THREADS, 1, 1)]
void main(uint3 dispatchID : SV_DispatchThreadID) {
    LightProbeManager manager;
    manager.chunkPositionsHeader = chunkPositionsHeader;
    manager.chunkPositions = chunkPositions;
    manager.lightChunks = lightChunks;
    manager.activeProbes = activeProbes;

    uint activeCapacity, activeStride;
    manager.activeProbes.GetDimensions(activeCapacity, activeStride);

    if (activeCapacity <= 1u)
        return;

    uint activeIndex = dispatchID.x;

    uint activeCount = min(
        manager.GetActiveProbeCount(),
        activeCapacity - 1u
    );

    if (activeIndex >= activeCount)
        return;

    uint globalIndex = manager.GetActiveProbeIndex(activeIndex);

    if (globalIndex == POINTER_EMPTY)
        return;

    LightProbeAddress address =
        manager.GetAddressFromGlobalIndex(globalIndex);

    uint chunkCount, chunkStride;
    manager.lightChunks.GetDimensions(chunkCount, chunkStride);

    if (address.chunkIndex >= chunkCount)
        return;

    LightProbe probe =
        manager.lightChunks[address.chunkIndex].probes[address.probeIndex];

    float3 history = probe.GetLight();

    uint3 coordinate = uint3(
        address.probeIndex & 15u,
        (address.probeIndex >> 4u) & 15u,
        (address.probeIndex >> 8u) & 15u
    );

    int3 probeBase =
        manager.lightChunks[address.chunkIndex].position * int(Chunk::WIDTH)
        + int3(coordinate) * int(LIGHT_PROBE_SPACING);

    uint2 emptyMask = uint2(
        ~uint(probe.solid),
        ~uint(probe.solid >> 32u)
    );

    uint lowEmptyCount = countbits(emptyMask.x);
    uint emptyCount = lowEmptyCount + countbits(emptyMask.y);

    CameraTransform camera = cameraTransform[0];

    float3 sunDirection = normalize(float3(sin(camera.time / 100.0), 1.0, cos(camera.time / 100.0)));

    uint probeSeed = PL_Hash(globalIndex);
    uint firstSample = uint(camera.frame) * uint(PROBE_LIGHTING_SAMPLES);

    float3 lighting = float3(0);

    if (emptyCount > 0u) {
        [loop]
        for (int i = 0; i < PROBE_LIGHTING_SAMPLES; ++i) {
            uint sequenceIndex = firstSample + uint(i) + 1u;

            float voxelSample = PL_Sample(
                sequenceIndex, 2u, probeSeed, 0u
            );

            float3 positionSample = float3(
                PL_Sample(sequenceIndex, 3u, probeSeed, 1u),
                PL_Sample(sequenceIndex, 5u, probeSeed, 2u),
                PL_Sample(sequenceIndex, 7u, probeSeed, 3u)
            );

            float2 directionSample = float2(
                PL_Sample(sequenceIndex, 11u, probeSeed, 4u),
                PL_Sample(sequenceIndex, 13u, probeSeed, 5u)
            );

            float3 origin = PL_ProbeOrigin(
                probeBase,
                emptyMask,
                lowEmptyCount,
                emptyCount,
                voxelSample,
                positionSample
            );

            float3 sampleLighting = PL_Sun(origin, sunDirection);

            sampleLighting += PL_Indirect(
                origin,
                PL_Sphere(directionSample),
                sunDirection,
                camera.time,
                sequenceIndex,
                probeSeed
            );

            lighting += sampleLighting;
        }
        lighting /= float(PROBE_LIGHTING_SAMPLES);
    }

    float3 accumulated = min(lerp(history, lighting, PL_HISTORY_RATE), float3(1));

    manager.SetLight(address, accumulated);
    manager.DeactivateProbe(probeBase);
}