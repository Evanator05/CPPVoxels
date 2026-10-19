#include "skyislandgeneration.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <vector>

namespace Generator {
namespace {

constexpr int ChunkSize = 64;
constexpr float Tau = 6.28318530718f;

template <typename T> T Clamp(T v, T lo, T hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

float Lerp(float a, float b, float t) { return a + (b - a) * t; }
float Smooth(float t) { return t * t * (3.0f - 2.0f * t); }

std::uint32_t Hash(std::uint32_t h)
{
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu;
    return h ^ (h >> 16);
}

std::uint32_t HashAt(int x, int y, int z, std::uint32_t seed)
{
    // Cast BEFORE multiplying: signed coordinate overflow is undefined in C++.
    return Hash(seed ^ (static_cast<std::uint32_t>(x) * 73856093u)
                     ^ (static_cast<std::uint32_t>(y) * 19349663u)
                     ^ (static_cast<std::uint32_t>(z) * 83492791u));
}

float Random(int x, int y, int z, std::uint32_t seed)
{
    // Exactly [0, 1), including after conversion to float.
    return static_cast<float>(HashAt(x, y, z, seed) >> 8) / 16777216.0f;
}

// Smooth value noise, kept local so no new noise-library dependency is needed.
float Noise(float x, float z, std::uint32_t seed)
{
    const int ix = static_cast<int>(std::floor(x));
    const int iz = static_cast<int>(std::floor(z));
    const float tx = Smooth(x - static_cast<float>(ix));
    const float tz = Smooth(z - static_cast<float>(iz));
    return Lerp(Lerp(Random(ix, 0, iz, seed), Random(ix + 1, 0, iz, seed), tx),
                Lerp(Random(ix, 0, iz + 1, seed), Random(ix + 1, 0, iz + 1, seed), tx), tz)
           * 2.0f - 1.0f;
}

float Fbm(float x, float z, std::uint32_t seed)
{
    return (Noise(x, z, seed) + 0.5f * Noise(x * 2.07f, z * 2.07f, seed + 1u)
           + 0.25f * Noise(x * 4.13f, z * 4.13f, seed + 2u)) / 1.75f;
}

Voxel Material(int r, int g, int b)
{
    Voxel v{};
    v.set_r(static_cast<std::uint8_t>(r));
    v.set_g(static_cast<std::uint8_t>(g));
    v.set_b(static_cast<std::uint8_t>(b));
    v.set_solid(true);
    return v;
}

struct Palette
{
    Voxel stone = Material(16, 17, 20);
    Voxel darkStone = Material(10, 11, 15);
    Voxel lightStone = Material(20, 20, 22);
    Voxel dirt = Material(17, 11, 6);
    Voxel grass = Material(10, 23, 7);
    Voxel moss = Material(8, 17, 8);
    Voxel wood = Material(13, 8, 4);
    Voxel leaves = Material(7, 21, 9);
    Voxel lightLeaves = Material(12, 26, 10);
    Voxel darkLeaves = Material(4, 15, 8);
    Voxel blossom = Material(29, 17, 23);
    Voxel stem = Material(9, 19, 5);
    Voxel flowers[4] = {Material(31, 24, 7), Material(28, 10, 19),
                        Material(19, 14, 30), Material(29, 29, 25)};
};

struct Column
{
    int top = -1;
    int bottom = -1;
    float edge = 0.0f;
    bool reserved = false;
};

struct TreeFootprint
{
    int x, z, radius;
};

struct Island
{
    int x0, z0, width, depth;
    float cx, cz, rx, rz, altitude, thickness;
    std::uint32_t seed;
    std::vector<Column> columns;
    std::vector<TreeFootprint> trees;

    Column* At(int x, int z)
    {
        if (x < x0 || z < z0 || x >= x0 + width || z >= z0 + depth)
            return nullptr;
        return &columns[static_cast<std::size_t>(z - z0) * width + (x - x0)];
    }
};

class SkyBuilder
{
public:
    SkyBuilder(VoxelManager& manager, int w, int h, int d, const SkyIslandSettings& options)
        : vm(manager), s(options), width(w * ChunkSize), height(h * ChunkSize),
          depth(d * ChunkSize), origin(options.originChunk * ChunkSize) {}

    void Run(int chunksX, int chunksY, int chunksZ)
    {
        if (s.verbose)
            std::printf("\n=== SKY ISLANDS: %d x %d x %d chunks; seed %u ===\n",
                        chunksX, chunksY, chunksZ, static_cast<unsigned>(s.seed));

        for (int x = 0; x < chunksX; ++x)
            for (int y = 0; y < chunksY; ++y)
                for (int z = 0; z < chunksZ; ++z)
                    vm.AllocateChunk(s.originChunk + glm::ivec3(x, y, z));

        // The engine requires all chunks to exist before building this map.
        vm.GenerateChunkOccupancyMap();

        const int cellsX = (width + s.islandSpacing - 1) / s.islandSpacing;
        const int cellsZ = (depth + s.islandSpacing - 1) / s.islandSpacing;
        for (int cz = 0; cz < cellsZ; ++cz)
        {
            for (int cx = 0; cx < cellsX; ++cx)
            {
                // Guarantee a central island when density is nonzero.
                const bool central = cx == cellsX / 2 && cz == cellsZ / 2;
                if (s.islandDensity == 0.0f || (!central &&
                    Random(cx, 0, cz, s.seed + 19u) >= s.islandDensity))
                    continue;

                Island island = MakeIsland(cx, cz, cellsX, cellsZ);
                BuildTerrain(island);
                PlantTrees(island);
                Decorate(island);
                ++islandCount;
            }
            if (s.verbose)
                std::printf("  Island rows: %d / %d; islands: %zu; trees: %zu\n",
                            cz + 1, cellsZ, islandCount, treeCount);
        }
        if (s.verbose)
            std::printf("=== COMPLETE: %zu islands, %zu trees, %zu voxel writes ===\n",
                        islandCount, treeCount, writes);
    }

private:
    VoxelManager& vm;
    const SkyIslandSettings& s;
    int width, height, depth;
    glm::ivec3 origin;
    Palette p;
    std::size_t islandCount = 0, treeCount = 0, writes = 0;

    void Set(int x, int y, int z, Voxel v, std::uint32_t seed)
    {
        // All terrain, branches, crowns and vines pass through this check.
        if (x < 0 || y < 0 || z < 0 || x >= width || y >= height || z >= depth)
            return;
        const int shade = static_cast<int>(HashAt(x, y, z, seed) % 3u) - 1;
        v.set_r(static_cast<std::uint8_t>(Clamp(static_cast<int>(v.r()) + shade, 0, 31)));
        v.set_g(static_cast<std::uint8_t>(Clamp(static_cast<int>(v.g()) + shade, 0, 31)));
        v.set_b(static_cast<std::uint8_t>(Clamp(static_cast<int>(v.b()) + shade, 0, 31)));
        vm.SetVoxel(glm::ivec3(x, y, z) + origin, v);
        ++writes;
    }

    void AboveGround(Island& a, int x, int y, int z, const Voxel& v, std::uint32_t seed)
    {
        Column* c = a.At(x, z);
        if (c && y > c->top)
            Set(x, y, z, v, seed);
    }

    Island MakeIsland(int cx, int cz, int cellsX, int cellsZ)
    {
        Island a;
        // Distribute remainder voxels across the cells; avoid tiny last tiles.
        a.x0 = static_cast<int>(static_cast<std::int64_t>(cx) * width / cellsX);
        a.z0 = static_cast<int>(static_cast<std::int64_t>(cz) * depth / cellsZ);
        a.width = static_cast<int>(static_cast<std::int64_t>(cx + 1) * width / cellsX) - a.x0;
        a.depth = static_cast<int>(static_cast<std::int64_t>(cz + 1) * depth / cellsZ) - a.z0;
        a.seed = HashAt(cx, 17, cz, s.seed);
        const float tileW = static_cast<float>(a.width);
        const float tileD = static_cast<float>(a.depth);
        a.cx = a.x0 + tileW * (0.5f + 0.12f * Noise(cx * 3.1f, cz * 2.7f, a.seed));
        a.cz = a.z0 + tileD * (0.5f + 0.12f * Noise(cx * 2.3f, cz * 3.7f, a.seed + 1u));
        const float scale = Lerp(0.70f, 1.25f, Random(cx, 2, cz, a.seed));
        a.rx = (std::min)(s.islandRadius * scale, tileW * 0.32f);
        a.rz = (std::min)(s.islandRadius * scale *
                         Lerp(0.78f, 1.18f, Random(cx, 3, cz, a.seed)), tileD * 0.32f);
        const int treeLimit = (std::min)(s.maxTreeHeight, height / 3);
        const float highestTop = static_cast<float>(height - treeLimit - 6);
        const float lowestTop = (std::min)(highestTop, (std::max)(25.0f, height * 0.38f));
        // Leave room for hills as well as complete tree crowns.
        a.altitude = Lerp(lowestTop, highestTop, Random(cx, 4, cz, a.seed)) - 5.0f;
        a.thickness = (std::min)((a.rx + a.rz) * 0.5f * s.undersideDepth,
                                a.altitude - 9.0f);
        a.columns.resize(static_cast<std::size_t>(a.width) * a.depth);
        return a;
    }

    void BuildTerrain(Island& a)
    {
        for (int z = a.z0 + 2; z < a.z0 + a.depth - 2; ++z)
        {
            for (int x = a.x0 + 2; x < a.x0 + a.width - 2; ++x)
            {
                const float u = (x - a.cx) / a.rx;
                const float v = (z - a.cz) / a.rz;
                const float warpX = 0.12f * Fbm(u * 1.7f, v * 1.7f, a.seed + 31u);
                const float warpZ = 0.12f * Fbm(u * 1.7f, v * 1.7f, a.seed + 41u);
                const float distance = std::sqrt((u + warpX) * (u + warpX) +
                                                 (v + warpZ) * (v + warpZ));
                const float outline = 0.93f + 0.14f * Fbm(u * 2.8f, v * 2.8f, a.seed + 51u);
                const float edge = 1.0f - distance / outline;
                if (edge <= 0.0f)
                    continue;

                const float hills = Fbm(x * 0.035f, z * 0.035f, a.seed + 61u);
                const float fine = Noise(x * 0.13f, z * 0.13f, a.seed + 62u);
                const float topF = a.altitude + 3.0f * Smooth(Clamp(edge * 2.0f, 0.0f, 1.0f))
                                  + s.surfaceRoughness * (hills * 9.0f + fine * 1.4f);
                // A broad stone shoulder narrows into a long irregular keel.
                const float keel = std::pow(edge, 0.78f);
                const float rib = 0.88f + 0.20f * Noise(u * 5.0f, v * 5.0f, a.seed + 71u);
                const float thickness = 3.0f + a.thickness * keel * rib;
                Column& c = *a.At(x, z);
                c.top = Clamp(static_cast<int>(std::floor(topF)), 12, height - 12);
                c.bottom = (std::max)(6, c.top - static_cast<int>(thickness));
                c.edge = edge;

                for (int y = c.bottom; y <= c.top; ++y)
                {
                    Voxel material = p.stone;
                    if (y == c.top)
                        material = edge < 0.045f ? p.moss : p.grass;
                    else if (y >= c.top - 3)
                        material = p.dirt;
                    else if (y < c.bottom + 2)
                        material = p.darkStone;
                    else if ((y + static_cast<int>(hills * 4.0f)) % 11 < 2)
                        material = p.lightStone;
                    else if (Noise(x * 0.09f, z * 0.09f + y * 0.07f, a.seed + 72u) < -0.30f)
                        material = p.darkStone;
                    Set(x, y, z, material, a.seed + 80u);
                }
            }
        }
    }

    bool GroundPatch(Island& a, int x, int z, int radius, int tolerance)
    {
        Column* center = a.At(x, z);
        if (!center || center->top < 0)
            return false;
        for (int dz = -radius; dz <= radius; ++dz)
            for (int dx = -radius; dx <= radius; ++dx)
            {
                Column* c = a.At(x + dx, z + dz);
                if (!c || c->top < 0 || c->reserved ||
                    std::abs(c->top - center->top) > tolerance)
                    return false;
            }
        return true;
    }

    void Reserve(Island& a, int x, int z, int radius)
    {
        for (int dz = -radius; dz <= radius; ++dz)
            for (int dx = -radius; dx <= radius; ++dx)
                if (dx * dx + dz * dz <= radius * radius)
                    if (Column* c = a.At(x + dx, z + dz))
                        c->reserved = true;
    }

    void Crown(Island& a, int x, int y, int z, float rx, float ry, float rz,
               bool pink, std::uint32_t seed)
    {
        const int bx = static_cast<int>(std::ceil(rx));
        const int by = static_cast<int>(std::ceil(ry));
        const int bz = static_cast<int>(std::ceil(rz));
        for (int dz = -bz; dz <= bz; ++dz)
            for (int dy = -by; dy <= by; ++dy)
                for (int dx = -bx; dx <= bx; ++dx)
                {
                    const float distance = dx * dx / (rx * rx) + dy * dy / (ry * ry)
                                           + dz * dz / (rz * rz);
                    const float n = Random(x + dx, y + dy, z + dz, seed);
                    // Keep a solid core; roughen just the outside of the crown.
                    if (distance > 0.86f + n * 0.14f)
                        continue;
                    Voxel leaf = pink ? p.blossom :
                                 (dy > 0 && n > 0.25f ? p.lightLeaves : p.leaves);
                    if (!pink && dy < -1 && n < 0.55f)
                        leaf = p.darkLeaves;
                    AboveGround(a, x + dx, y + dy, z + dz, leaf, seed);
                }
    }

    void Branch(Island& a, int x0, int y0, int z0, int x1, int y1, int z1,
                std::uint32_t seed)
    {
        // Connect through an axis at a time so wood stays face-connected.
        int x = x0, y = y0, z = z0;
        const int steps = (std::max)((std::max)(std::abs(x1 - x0), std::abs(y1 - y0)),
                                     std::abs(z1 - z0));
        AboveGround(a, x, y, z, p.wood, seed);
        for (int i = 1; i <= steps; ++i)
        {
            const float t = static_cast<float>(i) / steps;
            const int nx = static_cast<int>(std::lround(Lerp(static_cast<float>(x0), static_cast<float>(x1), t)));
            const int ny = static_cast<int>(std::lround(Lerp(static_cast<float>(y0), static_cast<float>(y1), t)));
            const int nz = static_cast<int>(std::lround(Lerp(static_cast<float>(z0), static_cast<float>(z1), t)));
            while (x != nx) { x += x < nx ? 1 : -1; AboveGround(a, x, y, z, p.wood, seed); }
            while (y != ny) { y += y < ny ? 1 : -1; AboveGround(a, x, y, z, p.wood, seed); }
            while (z != nz) { z += z < nz ? 1 : -1; AboveGround(a, x, y, z, p.wood, seed); }
        }
    }

    void Tree(Island& a, int x, int z, int treeHeight, std::uint32_t seed)
    {
        const int ground = a.At(x, z)->top;
        const bool pine = Random(x, 1, z, seed) < 0.24f;
        const bool pink = !pine && Random(x, 2, z, seed) < 0.14f;
        const int trunkHeight = pine ? treeHeight - 1 : treeHeight * 2 / 3;
        const int trunkRadius = treeHeight >= 23 ? 1 : 0;
        const int radius = (std::max)(3, treeHeight / 4);

        // Leaves first, wood last, so crowns cannot erase their own branches.
        if (pine)
        {
            for (int dy = treeHeight / 3; dy < treeHeight; ++dy)
            {
                const float t = static_cast<float>(treeHeight - dy) / treeHeight;
                const int r = (std::max)(0, static_cast<int>(t * radius * 1.4f) - (dy % 3 == 0 ? 1 : 0));
                for (int dz = -r; dz <= r; ++dz)
                    for (int dx = -r; dx <= r; ++dx)
                        if (dx * dx + dz * dz <= r * r)
                            AboveGround(a, x + dx, ground + 1 + dy, z + dz,
                                        dy % 3 == 1 ? p.leaves : p.darkLeaves, seed);
            }
        }
        else
        {
            Crown(a, x, ground + treeHeight - radius, z,
                  static_cast<float>(radius), static_cast<float>(radius),
                  static_cast<float>(radius), pink, seed + 1u);
            for (int b = 0; b < 4; ++b)
            {
                const float angle = b * Tau / 4.0f + Random(x, b, z, seed + 2u) * 0.9f;
                const int bx = x + static_cast<int>(std::lround(std::cos(angle) * radius * 0.85f));
                const int bz = z + static_cast<int>(std::lround(std::sin(angle) * radius * 0.85f));
                const int by = ground + trunkHeight - 1 + b % 2;
                Crown(a, bx, by + 1, bz, radius * 0.72f, radius * 0.65f,
                      radius * 0.72f, pink, seed + 10u + static_cast<std::uint32_t>(b));
            }
            for (int b = 0; b < 4; ++b)
            {
                const float angle = b * Tau / 4.0f + Random(x, b, z, seed + 2u) * 0.9f;
                Branch(a, x, ground + trunkHeight - 4, z,
                       x + static_cast<int>(std::lround(std::cos(angle) * radius * 0.85f)),
                       ground + trunkHeight - 1 + b % 2,
                       z + static_cast<int>(std::lround(std::sin(angle) * radius * 0.85f)), seed);
            }
        }

        for (int dz = -trunkRadius; dz <= trunkRadius; ++dz)
            for (int dx = -trunkRadius; dx <= trunkRadius; ++dx)
            {
                if (dx * dx + dz * dz > trunkRadius * trunkRadius)
                    continue;
                Column* base = a.At(x + dx, z + dz);
                if (!base || base->top < 0) continue;
                for (int y = base->top + 1; y <= ground + trunkHeight; ++y)
                    Set(x + dx, y, z + dz, p.wood, seed);
            }
        Reserve(a, x, z, radius * 2 + 1);
        ++treeCount;
    }

    bool TryTree(Island& a, int x, int z, std::uint32_t seed)
    {
        Column* c = a.At(x, z);
        if (!c || c->top < 0 || c->edge < 0.20f || !GroundPatch(a, x, z, 2, 2))
            return false;
        const int limit = (std::min)((std::min)(s.maxTreeHeight, height / 3), height - c->top - 2);
        if (limit < 8) return false;
        const int treeHeight = Clamp(static_cast<int>(limit * Lerp(0.65f, 1.0f,
                                            Random(x, c->top, z, seed))), 8, limit);
        const int crownExtent = 2 * (std::max)(3, treeHeight / 4) + 1;
        // Keep the entire crown inside its tile and inside the world.
        if (x - crownExtent < a.x0 + 1 || z - crownExtent < a.z0 + 1 ||
            x + crownExtent >= a.x0 + a.width - 1 || z + crownExtent >= a.z0 + a.depth - 1)
            return false;
        // Account for BOTH sizes: a large new crown must not erase a small
        // existing trunk. Crowns may still meet naturally near their edges.
        for (const TreeFootprint& other : a.trees)
        {
            const int dx = x - other.x, dz = z - other.z;
            const int separation = (std::max)(crownExtent, other.radius) + 2;
            if (dx * dx + dz * dz < separation * separation)
                return false;
        }
        Tree(a, x, z, treeHeight, seed);
        a.trees.push_back(TreeFootprint{x, z, crownExtent});
        return true;
    }

    void PlantTrees(Island& a)
    {
        if (s.treeDensity == 0.0f) return;
        for (int z = a.z0 + s.treeSpacing / 2; z < a.z0 + a.depth; z += s.treeSpacing)
            for (int x = a.x0 + s.treeSpacing / 2; x < a.x0 + a.width; x += s.treeSpacing)
            {
                const std::uint32_t seed = HashAt(x, 101, z, a.seed);
                const float forest = 0.5f + 0.5f * Fbm(x * 0.028f, z * 0.028f, a.seed + 110u);
                if (Random(x, 0, z, seed) >= s.treeDensity * (0.45f + 0.55f * forest))
                    continue;
                const int jitterX = static_cast<int>((Random(x, 1, z, seed) - 0.5f) * s.treeSpacing * 0.7f);
                const int jitterZ = static_cast<int>((Random(x, 2, z, seed) - 0.5f) * s.treeSpacing * 0.7f);
                // Resolve ground AFTER jittering, so trees are rooted correctly.
                TryTree(a, x + jitterX, z + jitterZ, seed);
            }
    }

    void Shrub(Island& a, int x, int z, std::uint32_t seed)
    {
        // Each foliage column touches its own ground even on small slopes.
        for (int dz = -2; dz <= 2; ++dz)
            for (int dx = -2; dx <= 2; ++dx)
            {
                if (dx * dx + dz * dz > 4) continue;
                Column* c = a.At(x + dx, z + dz);
                if (!c || c->top < 0) continue;
                const int h = (std::abs(dx) + std::abs(dz) <= 1) ? 3 : 2;
                for (int dy = 1; dy <= h; ++dy)
                    Set(x + dx, c->top + dy, z + dz, dy == h ? p.lightLeaves : p.leaves, seed);
            }
        Reserve(a, x, z, 3);
    }

    void Vine(Island& a, int x, int z, Column& c, std::uint32_t seed)
    {
        if (c.edge > 0.09f || Random(x, 0, z, seed) >= s.vineDensity)
            return;
        const int dx = x >= a.cx ? 1 : -1;
        const int dz = z >= a.cz ? 1 : -1;
        // Prefer a cardinal neighbor just outside the stone silhouette.
        int vx = x + dx, vz = z;
        Column* neighbor = a.At(vx, vz);
        if (!neighbor || neighbor->top >= 0)
        {
            vx = x; vz = z + dz; neighbor = a.At(vx, vz);
        }
        if (!neighbor || neighbor->top >= 0) return;
        const int length = 4 + static_cast<int>(Random(x, 1, z, seed) * 15.0f);
        Set(vx, c.top, vz, p.moss, seed); // Touches the island side.
        for (int y = c.top - 1; y >= (std::max)(3, c.top - length); --y)
            Set(vx, y, vz, (y % 4 == 0) ? p.leaves : p.moss, seed);
    }

    void Decorate(Island& a)
    {
        for (int z = a.z0; z < a.z0 + a.depth; ++z)
            for (int x = a.x0; x < a.x0 + a.width; ++x)
            {
                Column& c = *a.At(x, z);
                if (c.top < 0) continue;
                const std::uint32_t seed = a.seed + 200u;
                Vine(a, x, z, c, seed);
                if (c.reserved || c.edge < 0.08f) continue;

                const float moisture = 0.5f + 0.5f * Fbm(x * 0.055f, z * 0.055f, a.seed + 210u);
                if (Random(x, 0, z, seed + 1u) < s.vegetationDensity * moisture * 0.07f &&
                    GroundPatch(a, x, z, 2, 1))
                {
                    Shrub(a, x, z, seed);
                    continue;
                }
                if (Random(x, 0, z, seed + 2u) < s.flowerDensity * (0.3f + 0.7f * moisture))
                {
                    const int flower = static_cast<int>(Random(x / 5, 0, z / 5, seed + 3u) * 4.0f);
                    Set(x, c.top + 1, z, p.stem, seed);
                    Set(x, c.top + 2, z, p.flowers[flower], seed);
                }
                else if (Random(x, 0, z, seed + 4u) < s.vegetationDensity * (0.25f + moisture * 0.75f))
                {
                    const int blades = Random(x, 0, z, seed + 5u) < 0.30f ? 2 : 1;
                    for (int dy = 1; dy <= blades; ++dy)
                        Set(x, c.top + dy, z, p.stem, seed);
                }
            }
    }
};

void Validate(int w, int h, int d, const SkyIslandSettings& s)
{
    // Bound local coordinates so float noise and integer loop math stay sound.
    const int maxChunksPerAxis = 16384;
    if (w <= 0 || h <= 0 || d <= 0 || w > maxChunksPerAxis ||
        h > maxChunksPerAxis || d > maxChunksPerAxis)
        throw std::invalid_argument("Sky island dimensions must be 1..16384 chunks per axis.");
    const auto unit = [](float v) { return std::isfinite(v) && v >= 0.0f && v <= 1.0f; };
    if (!unit(s.islandDensity) || !unit(s.treeDensity) || !unit(s.vegetationDensity) ||
        !unit(s.flowerDensity) || !unit(s.vineDensity) || !unit(s.surfaceRoughness))
        throw std::invalid_argument("Sky island densities and surfaceRoughness must be finite and in [0, 1].");
    if (s.islandSpacing < 48 || s.islandSpacing > 1024 || s.treeSpacing < 6 ||
        s.treeSpacing > 96 || s.maxTreeHeight < 8 || s.maxTreeHeight > 96 ||
        !std::isfinite(s.islandRadius) || s.islandRadius < 8.0f || s.islandRadius > 384.0f ||
        !std::isfinite(s.undersideDepth) || s.undersideDepth < 0.1f || s.undersideDepth > 3.0f)
        throw std::invalid_argument("Sky island size settings are outside their supported ranges; see README.");

    const int dims[3] = {w, h, d};
    const int coords[3] = {s.originChunk.x, s.originChunk.y, s.originChunk.z};
    for (int i = 0; i < 3; ++i)
    {
        const std::int64_t first = static_cast<std::int64_t>(coords[i]) * ChunkSize;
        const std::int64_t last = first + static_cast<std::int64_t>(dims[i]) * ChunkSize - 1;
        if (first < (std::numeric_limits<int>::min)() || last > (std::numeric_limits<int>::max)())
            throw std::invalid_argument("Sky island origin and extent exceed integer world coordinates.");
    }
}

} // anonymous namespace

void GenerateSkyIslands(VoxelManager& vm, int chunkWidth, int chunkHeight, int chunkDepth,
                        const SkyIslandSettings& settings)
{
    Validate(chunkWidth, chunkHeight, chunkDepth, settings);
    SkyBuilder(vm, chunkWidth, chunkHeight, chunkDepth, settings)
        .Run(chunkWidth, chunkHeight, chunkDepth);
}

} // namespace Generator
