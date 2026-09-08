#include "facehashmap.slangh"

#define PROBE_THREADS 64
#define PROBE_GROUPS_PER_ROW 65535u

static const float CENTER_WEIGHT = 2.0;
static const float CARDINAL_WEIGHT = 1.0;
static const float CROSS_FACE_WEIGHT = 0.8;
static const float EDGE_REJECTION = 1.25;
static const float MIN_EDGE_WEIGHT = 0.50;

struct CameraTransform
{
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
RWStructuredBuffer<FaceEntry> smoothFaceHashmap;

int3 SmoothPlaneOffset(uint face, int a, int b) {
    if (face < 2u) return int3(0, a, b);
    if (face < 4u) return int3(a, 0, b);
    return int3(a, b, 0);
}

uint SmoothOppositeFace(uint face) {
    return face ^ 1u;
}

bool SmoothValidProbe(FaceReadResult result, uint expectedFrame, uint expectedFace) {
    if (result.slotIndex == INVALID_SLOT) return false;
    if (result.entry.frame != expectedFrame) return false;
    if (result.entry.face != expectedFace) return false;
    if (!all(isfinite(result.entry.lighting))) return false;
    if (any(result.entry.lighting < float3(0.0))) return false;
    return true;
}

FaceReadResult SmoothReadRaw(int3 position, uint face, bool swap, uint frame) {
    FaceKey key;
    key.position = position;
    key.face = face;
    key.swap = swap;
    key.frame = frame;
    return ReadKey(key, faceHashmap);
}

float SmoothEdgeWeight(float3 center, float3 sample) {
    if (EDGE_REJECTION <= 0.0)
        return 1.0;
    float3 compressedCenter = center / (float3(1.0) + center);
    float3 compressedSample = sample / (float3(1.0) + sample);
    float3 delta = compressedSample - compressedCenter;
    float difference = dot(delta, delta) * (1.0 / 3.0);
    float weight = 1.0 / (1.0 + difference * EDGE_REJECTION);
    return max(weight, MIN_EDGE_WEIGHT);
}

void SmoothAddProbe(int3 position, uint face, bool swap, uint frame, float baseWeight, float3 centerLighting, inout float3 lightingSum, inout float weightSum) {
    FaceReadResult result = SmoothReadRaw(position, face, swap, frame);

    if (!SmoothValidProbe(result, frame, face))
        return;
    
    float3 lighting = max(result.entry.lighting, float3(0.0));
    float weight = baseWeight * SmoothEdgeWeight(centerLighting, lighting);
    lightingSum += lighting * weight;
    weightSum += weight;
}

void SmoothAddCrossFaces(int3 position, uint centerFace, bool swap, uint frame, float3 centerLighting, inout float3 lightingSum, inout float weightSum) {
    uint oppositeFace = SmoothOppositeFace(centerFace);

    [unroll]
    for (uint sampleFace = 0u; sampleFace < 6u; ++sampleFace) {
        if (sampleFace == centerFace)
            continue;
        if (sampleFace == oppositeFace)
            continue;
        SmoothAddProbe(position, sampleFace, swap, frame, CROSS_FACE_WEIGHT, centerLighting, lightingSum, weightSum);
    }
}

[shader("compute")]
[numthreads(PROBE_THREADS, 1, 1)]
void main(uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID) {
    CameraTransform camera = cameraTransform[0];

    uint currentFrame = uint(camera.frame);
    bool currentSwap = IsEven(camera.frame);
    uint numEntries;
    uint stride;

    faceHashmap.GetDimensions(numEntries, stride);

    if ((numEntries & 1u) != 0u)
        return;

    uint capacity = numEntries / 2u;

    uint groupIndex = groupID.x + groupID.y * PROBE_GROUPS_PER_ROW;
    uint localSlot = groupIndex * uint(PROBE_THREADS) + groupThreadID.x;

    if (localSlot >= capacity)
        return;

    uint absoluteSlot = localSlot;

    if (currentSwap) {
        absoluteSlot += capacity;
    }

    FaceEntry currentEntry = faceHashmap[absoluteSlot];

    if (currentEntry.frame != currentFrame)
        return;

    uint face = currentEntry.face;

    if (face >= 6u)
        return;


    if (!all(isfinite(currentEntry.lighting)))
        return;


    if (any(currentEntry.lighting < float3(0.0)))
        return;

    int3 position = currentEntry.position;

    float3 centerLighting = max(currentEntry.lighting, float3(0.0));
    float3 lightingSum = centerLighting * CENTER_WEIGHT;

    float weightSum = CENTER_WEIGHT;
    SmoothAddProbe(position + SmoothPlaneOffset(face, -1, 0), face, currentSwap, currentFrame, CARDINAL_WEIGHT, centerLighting, lightingSum, weightSum);
    SmoothAddProbe(position + SmoothPlaneOffset(face, 1, 0), face, currentSwap, currentFrame, CARDINAL_WEIGHT, centerLighting, lightingSum, weightSum);
    SmoothAddProbe(position + SmoothPlaneOffset(face, 0, -1), face, currentSwap, currentFrame, CARDINAL_WEIGHT, centerLighting, lightingSum, weightSum);
    SmoothAddProbe(position + SmoothPlaneOffset(face, 0, 1), face, currentSwap, currentFrame, CARDINAL_WEIGHT, centerLighting, lightingSum, weightSum);
    SmoothAddCrossFaces(position, face, currentSwap, currentFrame, centerLighting, lightingSum, weightSum);

    float3 finalLighting = lightingSum / max(weightSum, 1.0e-8);

    if (!all(isfinite(finalLighting)))
        return;

    finalLighting = max( finalLighting, float3(0.0));
    currentEntry.lighting = finalLighting;

    smoothFaceHashmap[absoluteSlot] = currentEntry;
}