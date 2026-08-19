[[vk::binding(0, 1)]]
[format("r32f")]
RWTexture2D<float> halfDepth;

[[vk::binding(1, 1)]]
[format("r32f")]
RWTexture2D<float> fullDepth;

float MinUpscaleSample(RWTexture2D<float> halfResDepth, int2 highResCoord) {
    int2 halfCoord = highResCoord >> 1;

    uint imgW, imgH;
    halfDepth.GetDimensions(imgW, imgH);
    int2 halfSize = int2(imgW, imgH);

    int2 h0 = clamp(halfCoord, int2(0), halfSize - 1);
    int2 h1 = clamp(halfCoord + int2(1, 0), int2(0), halfSize - 1);
    int2 h2 = clamp(halfCoord + int2(0, 1), int2(0), halfSize - 1);
    int2 h3 = clamp(halfCoord + int2(1, 1), int2(0), halfSize - 1);

    float d0 = halfResDepth[h0];
    float d1 = halfResDepth[h1];
    float d2 = halfResDepth[h2];
    float d3 = halfResDepth[h3];

    return min(min(d0, d1), min(d2, d3));
}

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 gl_GlobalInvocationID: SV_DispatchThreadID) {
    int2 fullCoord = int2(gl_GlobalInvocationID.xy);

    uint imgW, imgH;
    fullDepth.GetDimensions(imgW, imgH);
    int2 fullSize = int2(imgW, imgH);

    if (any(fullCoord >= fullSize)) return;

    fullDepth[fullCoord] = MinUpscaleSample(halfDepth, fullCoord);
}