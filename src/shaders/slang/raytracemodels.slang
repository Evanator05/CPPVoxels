#include "modelraytrace.slangh"
#include "lightprobe.slangh"

[[vk::binding(0, 0)]] StructuredBuffer<ModelCameraTransform> cameras;
[[vk::binding(1, 0)]] StructuredBuffer<uint> modelVoxels;
[[vk::binding(2, 0)]] ByteAddressBuffer modelScene;
[[vk::binding(3, 0)]] StructuredBuffer<ChunkPositionsHeader> chunkPositionsHeader;
[[vk::binding(4, 0)]] StructuredBuffer<uint> chunkPositions;

[[vk::binding(0, 1)]]
[format("r32f")]
RWTexture2D<float> depthImage;

[[vk::binding(1, 1)]]
[format("rgba8")]
RWTexture2D<float4> albedoImage;

[[vk::binding(2, 1)]]
[format("rgba32i")]
RWTexture2D<int4> positionImage;

[[vk::binding(3, 1)]] RWStructuredBuffer<LightChunk> lightChunks;
[[vk::binding(4, 1)]] RWStructuredBuffer<uint> activeProbes;

[[vk::binding(0, 2)]] cbuffer ModelScene {
    uint4 sceneCounts;
    uint4 sceneOffsets;
};

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint width, height;
    depthImage.GetDimensions(width, height);
    if (id.x >= width || id.y >= height || sceneCounts.x == 0u)
        return;

    int2 pixel = int2(id.xy);
    float2 ndc = (float2(pixel) + 0.5) / float2(width, height) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    ndc.x *= float(width) / float(height);

    ModelCameraTransform camera = cameras[0];
    float3 direction = normalize(camera.right.xyz * ndc.x + camera.up.xyz * ndc.y + camera.forward.xyz);
    float worldDepth = depthImage[pixel];
    int4 worldHit = positionImage[pixel];

    ModelRayHit hit = TraceModels(
        camera.localPos.xyz,
        direction,
        ModelWorldLimit(worldDepth, worldHit.w != 0),
        sceneCounts.x,
        sceneCounts.y,
        sceneCounts.z,
        sceneCounts.w,
        modelVoxels,
        modelScene,
        sceneOffsets);
    
    if (!ModelWinsWorld(hit, worldDepth, worldHit.w != 0, worldHit.xyz, sceneOffsets.w != 0u))
        return;

    uint bits = hit.voxel;
    albedoImage[pixel] = float4(float3(bits & 31u, (bits >> 5u) & 31u, (bits >> 10u) & 31u) / 31.0, 1);
    int3 probeNormal = int3(sign(hit.normal));
    positionImage[pixel] = int4(hit.cell, int(MODEL_HIT_FLAG | (EncodeNormal26(probeNormal) + 1u)));
    depthImage[pixel] = hit.depth;

    LightProbeManager probes;
    probes.chunkPositionsHeader = chunkPositionsHeader;
    probes.chunkPositions = chunkPositions;
    probes.lightChunks = lightChunks;
    probes.activeProbes = activeProbes;

    probes.ActivateAndAddProbe(hit.cell);
    probes.ActivateAndAddProbe(hit.cell + probeNormal * LIGHT_PROBE_SPACING);
}
