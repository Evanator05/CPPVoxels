#pragma once

#include "testgeneration.h"
#include "glm/vec3.hpp"
#include <cstdint>

namespace Generator {

struct SkyIslandSettings
{
    std::uint32_t seed = 48291u;

    // Distances below are in voxels. Chunk dimensions are passed separately.
    int islandSpacing = 60;
    float islandRadius = 280.0f;
    float islandDensity = 0.95f;      // [0, 1]; zero produces an empty region.
    float undersideDepth = 0.95f;    // Depth relative to island radius.
    float surfaceRoughness = 0.65f;  // [0, 1].

    int treeSpacing = 14;
    int maxTreeHeight = 30;          // Total tree height, including its crown.
    float treeDensity = 0.68f;       // [0, 1]; probability per candidate.
    float vegetationDensity = 0.42f;
    float flowerDensity = 0.12f;
    float vineDensity = 0.22f;

    // Actual chunk coordinates to allocate. SetVoxel uses world coordinates.
    glm::ivec3 originChunk = glm::ivec3(0);
    bool verbose = true;
};

// Each chunk is 64 x 64 x 64 voxels, matching the supplied engine code.
// Example: (8, 3, 8) generates a 512 x 192 x 512 voxel region.
// Generate into an empty region: existing voxels are not erased.
// Invalid settings throw std::invalid_argument before modifying vm.
void GenerateSkyIslands(
    VoxelManager& vm,
    int chunkWidth,
    int chunkHeight,
    int chunkDepth,
    const SkyIslandSettings& settings = SkyIslandSettings{});

} // namespace Generator
