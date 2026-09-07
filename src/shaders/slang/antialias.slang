[[vk::binding(0, 1)]]
[format("r32f")]
RWTexture2D<float> depthImage;

[[vk::binding(1, 1)]]
[format("rgba8")]
RWTexture2D<float4> albedoImage;

[[vk::binding(2, 1)]]
[format("rgba8")]
RWTexture2D<float4> displayImage;

static const float kDepthEdgeThreshold = 0.05;   // relative depth diff where an edge starts
static const float kDepthEdgeRange     = 0.25;   // range over which it ramps 0 -> 1
static const float kDepthBleedGuard    = 0.08;   // extra relative-diff range, beyond
static const float kLumaEdgeThreshold    = 0.166;  // relative local-contrast trigger
static const float kLumaEdgeThresholdMin = 0.0833; // absolute floor, skips near-black regions
static const float kSubpixelFloor        = 0.25;   // guaranteed min. amount of isotropic blur
static const int kBlurRadius = 1; // 1 => 3x3 isotropic fallback blur, as in the original
static const float kLineBlurWeights[9] = {
    1.0 / 256.0, 8.0 / 256.0, 28.0 / 256.0, 56.0 / 256.0, 70.0 / 256.0,
    56.0 / 256.0, 28.0 / 256.0, 8.0 / 256.0, 1.0 / 256.0
};

float SampleDepth(int2 p, int2 imgSize) {
    p = clamp(p, int2(0, 0), imgSize - int2(1, 1));
    return depthImage[p];
}

float4 SampleAlbedo(int2 p, int2 imgSize) {
    p = clamp(p, int2(0, 0), imgSize - int2(1, 1));
    return albedoImage[p];
}

float Luma(float4 c) {
    return dot(c.rgb, float3(0.2126, 0.7152, 0.0722));
}

float RelativeDepthDiff(float a, float b) {
    return abs(a - b) / max(max(a, b), 1e-4);
}

float DepthSimilarityWeight(float tapDepth, float centerDepth) {
    float relDiff = RelativeDepthDiff(tapDepth, centerDepth);
    float over = max(relDiff - kDepthEdgeThreshold, 0.0);
    return saturate(1.0 - over / kDepthBleedGuard);
}

float4 IsotropicBlur(int2 pos, int2 imgSize, float centerDepth) {
    float4 sum = float4(0, 0, 0, 0);
    float weightSum = 0.0;

    [unroll]
    for (int by = -kBlurRadius; by <= kBlurRadius; by++)
    {
        [unroll]
        for (int bx = -kBlurRadius; bx <= kBlurRadius; bx++)
        {
            int2 tapPos = pos + int2(bx, by);
            float tapDepth = SampleDepth(tapPos, imgSize);
            float w = DepthSimilarityWeight(tapDepth, centerDepth);
            sum += SampleAlbedo(tapPos, imgSize) * w;
            weightSum += w;
        }
    }
    return sum / max(weightSum, 1e-4);
}

float4 DirectionalBlur(int2 pos, int2 imgSize, float centerDepth, int2 tangent) {
    float4 sum = float4(0, 0, 0, 0);
    float weightSum = 0.0;

    [unroll]
    for (int i = -4; i <= 4; i++)
    {
        int2 tapPos = pos + tangent * i;
        float tapDepth = SampleDepth(tapPos, imgSize);
        float w = kLineBlurWeights[i + 4] * DepthSimilarityWeight(tapDepth, centerDepth);
        sum += SampleAlbedo(tapPos, imgSize) * w;
        weightSum += w;
    }
    return sum / max(weightSum, 1e-4);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 gl_GlobalInvocationID: SV_DispatchThreadID) {
    int2 pos = int2(gl_GlobalInvocationID.xy);

    uint imgW, imgH;
    depthImage.GetDimensions(imgW, imgH);
    int2 imgSize = int2(imgW, imgH);

    if (pos.x >= imgSize.x || pos.y >= imgSize.y) return;

    float centerDepth = SampleDepth(pos, imgSize);
    float4 originalColor = SampleAlbedo(pos, imgSize);

    float depthN  = SampleDepth(pos + int2( 0,-1), imgSize);
    float depthS  = SampleDepth(pos + int2( 0, 1), imgSize);
    float depthE  = SampleDepth(pos + int2( 1, 0), imgSize);
    float depthW  = SampleDepth(pos + int2(-1, 0), imgSize);
    float depthNW = SampleDepth(pos + int2(-1,-1), imgSize);
    float depthNE = SampleDepth(pos + int2( 1,-1), imgSize);
    float depthSW = SampleDepth(pos + int2(-1, 1), imgSize);
    float depthSE = SampleDepth(pos + int2( 1, 1), imgSize);

    float4 colorN  = SampleAlbedo(pos + int2( 0,-1), imgSize);
    float4 colorS  = SampleAlbedo(pos + int2( 0, 1), imgSize);
    float4 colorE  = SampleAlbedo(pos + int2( 1, 0), imgSize);
    float4 colorW  = SampleAlbedo(pos + int2(-1, 0), imgSize);
    float4 colorNW = SampleAlbedo(pos + int2(-1,-1), imgSize);
    float4 colorNE = SampleAlbedo(pos + int2( 1,-1), imgSize);
    float4 colorSW = SampleAlbedo(pos + int2(-1, 1), imgSize);
    float4 colorSE = SampleAlbedo(pos + int2( 1, 1), imgSize);

    float lumaC  = Luma(originalColor);
    float lumaN  = Luma(colorN);
    float lumaS  = Luma(colorS);
    float lumaE  = Luma(colorE);
    float lumaW  = Luma(colorW);
    float lumaNW = Luma(colorNW);
    float lumaNE = Luma(colorNE);
    float lumaSW = Luma(colorSW);
    float lumaSE = Luma(colorSE);

    float maxDepthDiff = 0.0;
    maxDepthDiff = max(maxDepthDiff, RelativeDepthDiff(depthN,  centerDepth));
    maxDepthDiff = max(maxDepthDiff, RelativeDepthDiff(depthS,  centerDepth));
    maxDepthDiff = max(maxDepthDiff, RelativeDepthDiff(depthE,  centerDepth));
    maxDepthDiff = max(maxDepthDiff, RelativeDepthDiff(depthW,  centerDepth));
    maxDepthDiff = max(maxDepthDiff, RelativeDepthDiff(depthNW, centerDepth));
    maxDepthDiff = max(maxDepthDiff, RelativeDepthDiff(depthNE, centerDepth));
    maxDepthDiff = max(maxDepthDiff, RelativeDepthDiff(depthSW, centerDepth));
    maxDepthDiff = max(maxDepthDiff, RelativeDepthDiff(depthSE, centerDepth));

    float depthEdgeFactor = saturate((maxDepthDiff - kDepthEdgeThreshold) / kDepthEdgeRange);

    float lumaMin = min(lumaC, min(min(lumaN, lumaS), min(lumaE, lumaW)));
    float lumaMax = max(lumaC, max(max(lumaN, lumaS), max(lumaE, lumaW)));
    lumaMin = min(lumaMin, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    lumaMax = max(lumaMax, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float lumaRange = lumaMax - lumaMin;

    float lumaEdgeThresholdAbs = max(kLumaEdgeThresholdMin, lumaMax * kLumaEdgeThreshold);
    float lumaEdgeFactor = saturate((lumaRange - lumaEdgeThresholdAbs) / max(lumaEdgeThresholdAbs, 1e-4));

    float overallEdgeFactor = saturate(max(depthEdgeFactor, lumaEdgeFactor));

    if (overallEdgeFactor <= 0.0) {
        displayImage[pos] = originalColor;
        return;
    }

    float lumaGx = (lumaNE + 2.0 * lumaE + lumaSE) - (lumaNW + 2.0 * lumaW + lumaSW);
    float lumaGy = (lumaSW + 2.0 * lumaS + lumaSE) - (lumaNW + 2.0 * lumaN + lumaNE);
    float lumaGradMag = abs(lumaGx) + abs(lumaGy);

    bool  edgeIsVertical;        // true => edge runs vertically, blur along Y
    float orientationConfidence; // 0.5 = ambiguous (corner/dot), 1.0 = a clean direction

    if (lumaGradMag > 1e-5) {
        edgeIsVertical = abs(lumaGx) >= abs(lumaGy);
        orientationConfidence = max(abs(lumaGx), abs(lumaGy)) / lumaGradMag;
    }
    else {
        float depthGx = (depthNE + 2.0 * depthE + depthSE) - (depthNW + 2.0 * depthW + depthSW);
        float depthGy = (depthSW + 2.0 * depthS + depthSE) - (depthNW + 2.0 * depthN + depthNE);
        float depthGradMag = abs(depthGx) + abs(depthGy);

        edgeIsVertical = abs(depthGx) >= abs(depthGy);
        orientationConfidence = depthGradMag > 1e-8
            ? max(abs(depthGx), abs(depthGy)) / depthGradMag
            : 0.5;
    }

    int2 tangent = edgeIsVertical ? int2(0, 1) : int2(1, 0);

    float4 directional = DirectionalBlur(pos, imgSize, centerDepth, tangent);
    float4 isotropic   = IsotropicBlur(pos, imgSize, centerDepth);

    float directionalWeight = saturate((orientationConfidence - 0.5) * 2.0) * (1.0 - kSubpixelFloor);
    float4 blurredColor = lerp(isotropic, directional, directionalWeight);

    displayImage[pos] = lerp(originalColor, blurredColor, overallEdgeFactor);
}
