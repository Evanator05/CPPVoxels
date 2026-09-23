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

bool CombineHasOutwardAir(uint64_t solid, int3 baseFromVoxel, int3 outward) {
    uint64_t empty = ~solid;
    if (empty == 0ull)
        return false;

    int planeOffset = baseFromVoxel.x * outward.x
                    + baseFromVoxel.y * outward.y
                    + baseFromVoxel.z * outward.z;
    int minProjection = planeOffset;
    int maxProjection = planeOffset;

    minProjection += outward.x < 0 ? 3 * outward.x : 0;
    minProjection += outward.y < 0 ? 3 * outward.y : 0;
    minProjection += outward.z < 0 ? 3 * outward.z : 0;
    maxProjection += outward.x > 0 ? 3 * outward.x : 0;
    maxProjection += outward.y > 0 ? 3 * outward.y : 0;
    maxProjection += outward.z > 0 ? 3 * outward.z : 0;

    if (maxProjection <= 0)
        return false;
    if (minProjection > 0 || solid == 0ull)
        return true;

    int axisCount = (outward.x != 0 ? 1 : 0)
                  + (outward.y != 0 ? 1 : 0)
                  + (outward.z != 0 ? 1 : 0);
    if (axisCount == 1) {
        uint sliceStep = outward.x != 0 ? 1u : (outward.y != 0 ? 4u : 16u);
        uint64_t sliceMask = outward.x != 0 ? 0x1111111111111111ull : (outward.y != 0 ? 0x000F000F000F000Full : 0xFFFFull);
        int direction = outward.x + outward.y + outward.z;

        [unroll]
        for (uint slice = 0u; slice < 4u; ++slice) {
            if (planeOffset + int(slice) * direction > 0 &&
                (empty & (sliceMask << (slice * sliceStep))) != 0ull)
                return true;
        }
        return false;
    }

    [unroll]
    for (uint wordIndex = 0u; wordIndex < 2u; ++wordIndex) {
        uint word = uint(empty >> (wordIndex * 32u));
        [loop]
        while (word != 0u) {
            uint index = wordIndex * 32u + uint(firstbitlow(word));
            int projection = planeOffset
                           + int(index & 3u) * outward.x
                           + int((index >> 2u) & 3u) * outward.y
                           + int(index >> 4u) * outward.z;
            if (projection > 0)
                return true;
            word &= word - 1u;
        }
    }
    return false;
}

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
        LightProbeAddress adjacentAddress = currentAddress;

        if (any(adjacentLocal < int3(0)) || any(adjacentLocal >= int3(4))) {
            adjacentAddress = manager.GetProbeAddress(
                voxelPosition + directions[i]
            );

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

    // Trilinear coordinates relative to probe centers.
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

        int3 probeBase = neighbor * int(LIGHT_PROBE_SPACING);

        LightProbeAddress address = manager.GetProbeAddress(probeBase);
        if (!address.valid || address.chunkIndex >= chunkCount)
            continue;

        LightProbe probe = manager.lightChunks[address.chunkIndex].probes[address.probeIndex];
        if (probe.solid == 0xFFFFFFFFFFFFFFFFull)
            continue;

        if (hasOutward && !CombineHasOutwardAir(probe.solid, probeBase - voxelPosition, int3(outward)))
            continue;

        float3 probeLight = probe.GetLight();
        if (!all(isfinite(probeLight)))
            continue;

        lightingSum += probeLight * weight;
        totalWeight += weight;
    }

    float3 lighting = totalWeight > 0.0 ? lightingSum / totalWeight : float3(0);

    float exposure = 1.0;
    float3 hdr = max(albedo.rgb * lighting, float3(0)) * exposure;
    float3 mapped = hdr / (float3(1.0) + hdr);
    displayImage[pixel] = float4(mapped, albedo.a);
}
