[[vk::binding(0, 1)]]
RWStructuredBuffer<uint> placedProbes;

[[vk::binding(1, 1)]]
RWStructuredBuffer<uint> indirectArgs;

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint probeCount = placedProbes[0];

    indirectArgs[0] = (probeCount + 63u) / 64u;
    indirectArgs[1] = 1u;
    indirectArgs[2] = 1u;
}