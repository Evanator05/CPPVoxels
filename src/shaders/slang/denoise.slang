#include "facehashmap.slangh"

#define PROBE_THREADS 64

static const float CENTER_WEIGHT = 2.0;
static const float CARDINAL_WEIGHT = 2.0;
static const float CROSS_FACE_WEIGHT = 0.4;

static const float EDGE_REJECTION = 1.25;
static const float MIN_EDGE_WEIGHT = 0.2;

struct CameraTransform {
    int3 chunkPos;
    float3 localPos;
    float3x3 rotation;
    float time;
    int frame;
};

[[vk::binding(0, 0)]]
StructuredBuffer<CameraTransform> cameraTransform;

[[vk::binding(0, 1)]]
RWStructuredBuffer<FaceEntry> faceHashmap;

[[vk::binding(1, 1)]]
RWStructuredBuffer<uint> placedProbes;

[[vk::binding(2, 1)]]
RWStructuredBuffer<FaceEntry> smoothFaceHashmap;

int3 SmoothPlaneOffset(uint face, int a, int b) {
    if (face < 2u) return int3(0, a, b);
    if (face < 4u) return int3(a, 0, b);
    return int3(a, b, 0);
}

uint SmoothSelectCrossFace(uint centerFace, uint choice) {
    choice &= 3u;
    if (centerFace < 2u)
        return choice + 2u;
    if (centerFace < 4u) 
        return choice < 2u ? choice : choice + 2u;
    return choice;
}

float SmoothEdgeWeight( float3 center, float3 sample) {
    float centerPeak = max(center.x, max(center.y, center.z));
    float samplePeak = max(sample.x, max(sample.y, sample.z));
    float scale = 1.0 + max(centerPeak, samplePeak);
    float invScale = rcp(scale);
    float3 delta = (sample - center) * invScale;
    float difference = dot(delta, delta) * (1.0 / 3.0);
    float weight = rcp(1.0 + difference * EDGE_REJECTION);
    return max(weight, MIN_EDGE_WEIGHT);
}

bool SmoothTryReadLighting(int3 position, uint face, bool swap, uint frame, out float3 lighting) {
    FaceKey key;
    key.position = position;
    key.face = face;
    key.swap = swap;
    key.frame = frame;

    FaceReadResult result = ReadKey(key, faceHashmap);

    if (result.slotIndex == INVALID_SLOT)
        return false;
    if (result.entry.frame != frame)
        return false;

    if (result.entry.face != face)
        return false;

    lighting = max(result.entry.lighting, float3(0.0));
    return true;
}

void SmoothAddProbe(int3 position, uint face, bool swap, uint frame, float baseWeight, float3 centerLighting, inout float3 lightingSum, inout float weightSum) {
    float3 lighting;

    if (!SmoothTryReadLighting(position, face, swap, frame, lighting))
        return;
    
    float weight = baseWeight * SmoothEdgeWeight(centerLighting, lighting);
    lightingSum += lighting * weight;
    weightSum += weight;
}

[shader("compute")]
[numthreads(PROBE_THREADS, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint placedIndex = dispatchThreadID.x;
    uint placedBufferCount;
    uint placedStride;
    placedProbes.GetDimensions(placedBufferCount, placedStride);

    if (placedBufferCount <= 1u)
        return;

    uint placedCount = min(placedProbes[0], placedBufferCount - 1u);

    if (placedIndex >= placedCount)
        return;

    uint absoluteSlot = placedProbes[placedIndex + 1u];

    if (absoluteSlot == INVALID_SLOT)
        return;

    int frameValue = cameraTransform[0].frame;
    uint currentFrame = uint(frameValue);
    bool currentSwap = IsEven(frameValue);
    FaceEntry currentEntry = faceHashmap[absoluteSlot];

    if (currentEntry.frame != currentFrame)
        return;

    uint face = currentEntry.face;

    if (face >= 6u)
        return;

    int3 position = currentEntry.position;
    float3 centerLighting = max(currentEntry.lighting, float3(0.0));
    float3 lightingSum = centerLighting * CENTER_WEIGHT;

    float weightSum = CENTER_WEIGHT;
    uint sampleHash = Hash(position, face);
    int directionA = (sampleHash & 1u) ? 1 : -1;
    SmoothAddProbe(position + SmoothPlaneOffset(face, directionA, 0), face, currentSwap, currentFrame, CARDINAL_WEIGHT, centerLighting, lightingSum, weightSum);

    int directionB = (sampleHash & 2u) ? 1 : -1;
    SmoothAddProbe(position + SmoothPlaneOffset(face, 0, directionB), face, currentSwap, currentFrame, CARDINAL_WEIGHT, centerLighting, lightingSum, weightSum);
    uint crossChoice = (sampleHash >> 2u) & 3u;
    uint crossFace = SmoothSelectCrossFace(face, crossChoice);
    SmoothAddProbe(position, crossFace, currentSwap, currentFrame, CROSS_FACE_WEIGHT, centerLighting, lightingSum, weightSum);

    float invWeight = rcp(max(weightSum, 1.0e-8));

    float3 finalLighting = lightingSum * invWeight;

    currentEntry.lighting = max(finalLighting, float3(0.0));

    smoothFaceHashmap[absoluteSlot] = currentEntry;
}