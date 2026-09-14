#include "lightprobe.slangh"

[[vk::binding(0, 0)]]
Texture2D<float4> albedoImage;

[[vk::binding(1, 0)]]
Texture2D<int4> positionImage;

[[vk::binding(2, 0)]]
StructuredBuffer<ChunkPositionsHeader> chunkPositionsHeader;

[[vk::binding(3, 0)]]
StructuredBuffer<uint32_t> chunkPositions;

[[vk::binding(0, 1)]]
[format("rgba8")]
RWTexture2D<float4> displayImage;

[[vk::binding(1, 1)]]
RWStructuredBuffer<LightChunk> lightChunks;

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dispatchID : SV_DispatchThreadID) {
    uint width, height;
    displayImage.GetDimensions(width, height);
    if (dispatchID.x >= width || dispatchID.y >= height)
        return;

    int2 pixel = int2(dispatchID.xy);
    float4 albedo = albedoImage[pixel];
    
    int4 hitPosition = positionImage[pixel];

    if (hitPosition.w == 0) {
        displayImage[pixel] = albedo;
        return;
    }

    LightProbeManager manager;
    manager.chunkPositionsHeader = chunkPositionsHeader;
    manager.chunkPositions = chunkPositions;
    manager.lightChunks = lightChunks;

    uint chunkCount, chunkStride;
    manager.lightChunks.GetDimensions(chunkCount, chunkStride);

    int3 voxelPosition = hitPosition.xyz;
    float3 voxelCenter = float3(voxelPosition) + 0.5;

    const int3 directions[6] = {
        int3(1, 0, 0), int3(-1, 0, 0),
        int3(0, 1, 0), int3(0, -1, 0),
        int3(0, 0, 1), int3(0, 0, -1)
    };

    int3 localVoxel = voxelPosition & int3(3);
    LightProbeAddress currentAddress = manager.GetProbeAddress(voxelPosition);
    uint64_t currentSolid = 0ull;
    if (currentAddress.valid && currentAddress.chunkIndex < chunkCount) {
        currentSolid = manager.lightChunks[currentAddress.chunkIndex].probes[currentAddress.probeIndex].solid;
    }

    float3 outward = float3(0);

    [unroll]
    for (uint i = 0u; i < 6u; ++i) {
        int3 adjacentLocal = localVoxel + directions[i];
        uint64_t solid = currentSolid;

        if (any(adjacentLocal < int3(0)) || any(adjacentLocal >= int3(4))) {
            LightProbeAddress adjacentAddress = manager.GetProbeAddress(voxelPosition + directions[i]);

            solid = 0ull;
            if (adjacentAddress.valid && adjacentAddress.chunkIndex < chunkCount) {
                solid = manager.lightChunks[adjacentAddress.chunkIndex].probes[adjacentAddress.probeIndex].solid;
            }

            adjacentLocal &= int3(3);
        }

        uint bitIndex = uint(adjacentLocal.x + adjacentLocal.y * 4 + adjacentLocal.z * 16);
        if (((solid >> bitIndex) & 1ull) == 0ull)
            outward += float3(directions[i]);
    }

    bool hasOutward = dot(outward, outward) > 0.0;

    float3 probeGrid = voxelCenter / float(LIGHT_PROBE_SPACING) - 0.5;
    int3 baseProbe = int3(floor(probeGrid));
    float3 blend = frac(probeGrid);

    float3 lightingSum = float3(0);
    float totalWeight = 0.0;

    [unroll]
    for (uint i = 0u; i < 8u; ++i) {
        int3 offset = int3(int(i & 1u), int((i >> 1u) & 1u), int(i >> 2u));
        int3 neighbor = baseProbe + offset;

        float3 axisWeights = lerp(1.0 - blend, blend, float3(offset));
        float weight = axisWeights.x * axisWeights.y * axisWeights.z;
        if (weight <= 0.0)
            continue;

        float3 probeCenter = (float3(neighbor) + 0.5) * float(LIGHT_PROBE_SPACING);

        if (hasOutward && dot(probeCenter - voxelCenter, outward) < 0.0)
            continue;

        LightProbeAddress address = manager.GetProbeAddress(
            neighbor * int(LIGHT_PROBE_SPACING)
        );
        if (!address.valid || address.chunkIndex >= chunkCount)
            continue;

        LightProbe probe =
            manager.lightChunks[address.chunkIndex].probes[address.probeIndex];
        if (probe.solid == 0xFFFFFFFFFFFFFFFFull)
            continue;

        uint normalIndex = probe.GetNormal();
        if (hasOutward && normalIndex < 26u) {
            float3 probeNormal = float3(DecodeNormal26(normalIndex));
            if (dot(probeNormal, outward) < 0.0)
                continue;
        }

        lightingSum += (float3(probe.GetLight())) * weight;
        totalWeight += weight;
    }
    float3 lighting = totalWeight > 0.0
        ? lightingSum / totalWeight
        : float3(0);

    float exposure = 1.0;

    float3 hdr = max(albedo.rgb * lighting, float3(0)) * exposure;

    float3 mapped = hdr / (float3(1.0) + hdr);

    displayImage[pixel] = float4(albedo.rgb * lighting, albedo.a);
}
