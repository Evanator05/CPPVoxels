#include "facehashmap.slangh"

[[vk::binding(0, 1)]]
RWStructuredBuffer<uint> placedProbes;

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint3 gl_GlobalInvocationID: SV_DispatchThreadID) {
    ResetPlacedIndexList(placedProbes);
}