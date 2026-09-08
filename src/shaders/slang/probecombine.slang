#include "facehashmap.slangh"

[[vk::binding(0, 0)]]
Texture2D<float4> albedoImage;

[[vk::binding(1, 0)]]
Texture2D<uint> probeIndexImage;

[[vk::binding(2, 0)]]
StructuredBuffer<FaceEntry> faceHashmap;

[[vk::binding(0, 1)]]
[format("rgba8")]
RWTexture2D<float4> displayImage;

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dispatchID : SV_DispatchThreadID) {
    uint width, height;
    displayImage.GetDimensions(width, height);
    if (dispatchID.x >= width || dispatchID.y >= height) return;

    int2 pos = int2(dispatchID.xy);
    float4 albedo = albedoImage.Load(int3(pos, 0));
    uint probeIndex = probeIndexImage.Load(int3(pos, 0));

    if (probeIndex == INVALID_SLOT) {
        displayImage[pos] = albedo;
        return;
    }

    uint entryCount, stride;
    faceHashmap.GetDimensions(entryCount, stride);
    if (probeIndex >= entryCount) {
        displayImage[pos] = albedo;
        return;
    }

    if (faceHashmap[probeIndex].face >= 6u) {
        displayImage[pos] = albedo;
        return;
    }

    float3 lighting = faceHashmap[probeIndex].lighting;
    float3 color = max(albedo.rgb * lighting, float3(0));
    displayImage[pos] = float4(color, albedo.a);
}