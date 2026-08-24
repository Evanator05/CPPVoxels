#include "raytrace.slangh"

struct CameraTransform {
    int3 chunkPos;
    float3 localPos;
    float3x3 rotation;
    float time;
};

[[vk::binding(0, 0)]]
StructuredBuffer<ContreeNode> contreeNodes;

[[vk::binding(1, 0)]]
StructuredBuffer<Chunk> chunks;

[[vk::binding(2, 0)]]
StructuredBuffer<ChunkPositionsHeader> chunkPositionsHeader;

[[vk::binding(3, 0)]]
StructuredBuffer<uint32_t> chunkPositions;

[[vk::binding(4, 0)]]
StructuredBuffer<CameraTransform> cameraTransform;

[[vk::binding(0, 1)]]
[format("r32f")]
RWTexture2D<float> depthImage;

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 gl_GlobalInvocationID: SV_DispatchThreadID)
{
    int2 pos = int2(gl_GlobalInvocationID.xy);

    uint imgW, imgH;
    depthImage.GetDimensions(imgW, imgH);
    int2 imgSize = int2(imgW, imgH);

    if (pos.x >= imgSize.x || pos.y >= imgSize.y) return;

    float maxDepth = imgSize.y * 0.5;

    float2 ndc = GetNDC(pos, imgSize);

    CameraTransform camera = cameraTransform[0];
    Ray ray;
    ray.origin = camera.localPos;
    ray.direction = normalize(mul(camera.rotation, float3(ndc, 1.0)));

    float3 color = float3(0, 0, 0);
    TraceResult result = TraceWorld(ray, maxDepth, contreeNodes, chunks, chunkPositionsHeader, chunkPositions);

    depthImage[pos] = min(max(result.depth-0.1, 0), maxDepth);
}