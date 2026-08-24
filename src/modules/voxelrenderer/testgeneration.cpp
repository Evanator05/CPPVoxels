#include "testgeneration.h"
#include "glm/vec3.hpp"
#include "glm/vec2.hpp"
#include "glm/mat3x3.hpp"
#include <glm/ext/matrix_relational.hpp> // Often required for matrix extensions
#include <glm/gtc/matrix_transform.hpp>
#include <math.h>
#include <algorithm>

namespace Generator {

void GenerateWorld(VoxelManager& vm)
{
    // ============================================================
    // WORLD
    //
    // 30 x 3 x 30 chunks
    // 64 voxels per chunk
    //
    // 1920 x 192 x 1920 voxels
    // ============================================================

    constexpr int CHUNK_SIZE = 64;

    constexpr int WORLD_X = 30 * CHUNK_SIZE;
    constexpr int WORLD_Y = 3  * CHUNK_SIZE;
    constexpr int WORLD_Z = 30 * CHUNK_SIZE;

    constexpr int SEA_LEVEL = 42;

    // Terrain is sampled every 4 voxels.
    //
    // This is a good compromise:
    //
    // 1x1  = extremely expensive
    // 2x2  = smoother, more expensive
    // 4x4  = good balance
    // 8x8  = fast, but visibly blocky
    //
    constexpr int TERRAIN_STEP = 4;


    // ============================================================
    // ALLOCATE WORLD
    // ============================================================

    for (int x = 0; x < 30; ++x)
    {
        for (int y = 0; y < 3; ++y)
        {
            for (int z = 0; z < 30; ++z)
            {
                vm.AllocateChunk(
                    glm::ivec3(x, y, z)
                );
            }
        }
    }

    vm.GenerateChunkOccupancyMap();


    // ============================================================
    // MATERIALS
    // ============================================================

    auto voxel =
        [](uint8_t r,
           uint8_t g,
           uint8_t b,
           bool solid = true)
    {
        Voxel v{};

        v.set_r(r);
        v.set_g(g);
        v.set_b(b);
        v.set_solid(solid);

        return v;
    };

    Voxel air =
        voxel(0, 0, 0, false);

    Voxel stone =
        voxel(14, 14, 15);

    Voxel stoneDark =
        voxel(8, 8, 9);

    Voxel stoneLight =
        voxel(20, 20, 20);

    Voxel dirt =
        voxel(16, 10, 5);

    Voxel dirtDark =
        voxel(11, 7, 4);

    Voxel grass =
        voxel(8, 24, 5);

    Voxel grassDark =
        voxel(5, 18, 4);

    Voxel sand =
        voxel(27, 23, 13);

    Voxel sandDark =
        voxel(22, 18, 10);

    Voxel snow =
        voxel(27, 28, 29);

    Voxel water =
        voxel(3, 12, 28);

    Voxel wood =
        voxel(18, 10, 4);

    Voxel woodDark =
        voxel(11, 6, 3);

    Voxel leaves =
        voxel(5, 20, 6);

    Voxel leavesDark =
        voxel(3, 14, 4);

    Voxel leavesLight =
        voxel(8, 27, 8);

    Voxel brick =
        voxel(23, 16, 10);

    Voxel roof =
        voxel(24, 5, 4);

    Voxel glass =
        voxel(8, 20, 28);

    Voxel road =
        voxel(13, 11, 8);

    Voxel torch =
        voxel(31, 20, 4);

    Voxel gold =
        voxel(28, 20, 4);


    // ============================================================
    // HELPERS
    // ============================================================

    auto Cube =
        [&](glm::ivec3 a,
            glm::ivec3 b,
            const Voxel& v)
    {
        vm.FillVoxels(
            a,
            b,
            v
        );
    };


    // ============================================================
    // NOISE
    // ============================================================

    FastNoiseLite terrainNoise;
    terrainNoise.SetNoiseType(
        FastNoiseLite::NoiseType_OpenSimplex2
    );
    terrainNoise.SetSeed(48291);
    terrainNoise.SetFrequency(0.0025f);

    FastNoiseLite detailNoise;
    detailNoise.SetNoiseType(
        FastNoiseLite::NoiseType_OpenSimplex2
    );
    detailNoise.SetSeed(192837);
    detailNoise.SetFrequency(0.012f);

    FastNoiseLite biomeNoise;
    biomeNoise.SetNoiseType(
        FastNoiseLite::NoiseType_OpenSimplex2
    );
    biomeNoise.SetSeed(837261);
    biomeNoise.SetFrequency(0.0018f);

    FastNoiseLite temperatureNoise;
    temperatureNoise.SetNoiseType(
        FastNoiseLite::NoiseType_OpenSimplex2
    );
    temperatureNoise.SetSeed(92831);
    temperatureNoise.SetFrequency(0.0015f);

    FastNoiseLite moistureNoise;
    moistureNoise.SetNoiseType(
        FastNoiseLite::NoiseType_OpenSimplex2
    );
    moistureNoise.SetSeed(19231);
    moistureNoise.SetFrequency(0.0017f);

    FastNoiseLite treeNoise;
    treeNoise.SetNoiseType(
        FastNoiseLite::NoiseType_OpenSimplex2
    );
    treeNoise.SetSeed(912837);
    treeNoise.SetFrequency(0.004f);

    FastNoiseLite structureNoise;
    structureNoise.SetNoiseType(
        FastNoiseLite::NoiseType_OpenSimplex2
    );
    structureNoise.SetSeed(723819);
    structureNoise.SetFrequency(0.002f);


    // ============================================================
    // BIOMES
    // ============================================================

    enum class Biome
    {
        Plains,
        Forest,
        Desert,
        Swamp,
        Mountain
    };


    auto GetBiome =
        [&](int x,
            int z,
            int height) -> Biome
    {
        float fx =
            static_cast<float>(x);

        float fz =
            static_cast<float>(z);

        float moisture =
            moistureNoise.GetNoise(
                fx,
                fz
            );

        float temperature =
            temperatureNoise.GetNoise(
                fx,
                fz
            );

        float biome =
            biomeNoise.GetNoise(
                fx,
                fz
            );

        if (height > 100)
            return Biome::Mountain;

        if (temperature > 0.45f &&
            moisture < -0.1f)
        {
            return Biome::Desert;
        }

        if (moisture > 0.45f &&
            height < SEA_LEVEL + 12)
        {
            return Biome::Swamp;
        }

        if (moisture > 0.05f &&
            biome > -0.15f)
        {
            return Biome::Forest;
        }

        return Biome::Plains;
    };


    // ============================================================
    // RAW TERRAIN HEIGHT
    //
    // This is only evaluated at terrain sample points.
    // ============================================================

    auto GetHeightFloat =
        [&](float fx,
            float fz) -> float
    {
        float large =
            terrainNoise.GetNoise(
                fx,
                fz
            );

        float detail =
            detailNoise.GetNoise(
                fx,
                fz
            );

        float ridgeNoise =
            terrainNoise.GetNoise(
                fx * 0.42f,
                fz * 0.42f
            );

        float ridges =
            1.0f -
            std::abs(ridgeNoise);

        ridges *= ridges;

        float h =
            55.0f;

        h +=
            large * 24.0f;

        h +=
            detail * 7.0f;

        h +=
            ridges * 45.0f;

        h +=
            sinf(fx * 0.003f) *
            7.0f;

        h +=
            sinf(fz * 0.004f) *
            6.0f;

        return glm::clamp(
            h,
            8.0f,
            static_cast<float>(WORLD_Y - 20)
        );
    };


    // ============================================================
    // TERRAIN SAMPLE GRID
    //
    // Instead of calculating noise for every voxel column,
    // calculate it once every TERRAIN_STEP voxels.
    // ============================================================

    constexpr int SAMPLE_X =
        WORLD_X / TERRAIN_STEP + 1;

    constexpr int SAMPLE_Z =
        WORLD_Z / TERRAIN_STEP + 1;

    std::vector<float> terrainSamples(
        SAMPLE_X * SAMPLE_Z
    );


    auto SampleIndex =
        [&](int sx, int sz)
    {
        return sz * SAMPLE_X + sx;
    };


    for (int sz = 0; sz < SAMPLE_Z; ++sz)
    {
        for (int sx = 0; sx < SAMPLE_X; ++sx)
        {
            float x =
                static_cast<float>(
                    std::min(
                        sx * TERRAIN_STEP,
                        WORLD_X
                    )
                );

            float z =
                static_cast<float>(
                    std::min(
                        sz * TERRAIN_STEP,
                        WORLD_Z
                    )
                );

            terrainSamples[
                SampleIndex(sx, sz)
            ] =
                GetHeightFloat(
                    x,
                    z
                );
        }
    }


    // ============================================================
    // SMOOTH HEIGHT
    //
    // Bilinear interpolation between the terrain samples.
    //
    // This is the important part:
    //
    //       sample ----- sample
    //          |           |
    //          | interpolate
    //          |           |
    //       sample ----- sample
    //
    // So the terrain does not abruptly jump between samples.
    // ============================================================

    auto GetHeight =
        [&](int x,
            int z) -> int
    {
        x =
            glm::clamp(
                x,
                0,
                WORLD_X - 1
            );

        z =
            glm::clamp(
                z,
                0,
                WORLD_Z - 1
            );


        int sx =
            x / TERRAIN_STEP;

        int sz =
            z / TERRAIN_STEP;


        int sx1 =
            std::min(
                sx + 1,
                SAMPLE_X - 1
            );

        int sz1 =
            std::min(
                sz + 1,
                SAMPLE_Z - 1
            );


        float tx =
            static_cast<float>(
                x % TERRAIN_STEP
            ) /
            static_cast<float>(
                TERRAIN_STEP
            );

        float tz =
            static_cast<float>(
                z % TERRAIN_STEP
            ) /
            static_cast<float>(
                TERRAIN_STEP
            );


        float h00 =
            terrainSamples[
                SampleIndex(
                    sx,
                    sz
                )
            ];

        float h10 =
            terrainSamples[
                SampleIndex(
                    sx1,
                    sz
                )
            ];

        float h01 =
            terrainSamples[
                SampleIndex(
                    sx,
                    sz1
                )
            ];

        float h11 =
            terrainSamples[
                SampleIndex(
                    sx1,
                    sz1
                )
            ];


        // Smoothstep interpolation instead of
        // linear interpolation.
        //
        // This makes the transitions between terrain
        // samples much less mechanical.

        float smoothX =
            tx * tx *
            (3.0f - 2.0f * tx);

        float smoothZ =
            tz * tz *
            (3.0f - 2.0f * tz);


        float h0 =
            glm::mix(
                h00,
                h10,
                smoothX
            );

        float h1 =
            glm::mix(
                h01,
                h11,
                smoothX
            );

        float h =
            glm::mix(
                h0,
                h1,
                smoothZ
            );


        return static_cast<int>(
            std::floor(h)
        );
    };


    // ============================================================
    // TREE
    // ============================================================

    auto Tree =
        [&](int x,
            int y,
            int z,
            int height,
            Biome biome)
    {
        Voxel trunk =
            biome == Biome::Swamp
                ? woodDark
                : wood;

        Voxel leaf =
            biome == Biome::Swamp
                ? leavesDark
                : leaves;

        Cube(
            glm::ivec3(
                x - 1,
                y,
                z - 1
            ),
            glm::ivec3(
                x + 2,
                y + height,
                z + 2
            ),
            trunk
        );

        Cube(
            glm::ivec3(
                x - 4,
                y + height - 3,
                z - 4
            ),
            glm::ivec3(
                x + 5,
                y + height + 2,
                z + 5
            ),
            leaf
        );

        Cube(
            glm::ivec3(
                x - 3,
                y + height + 1,
                z - 3
            ),
            glm::ivec3(
                x + 4,
                y + height + 5,
                z + 4
            ),
            leavesLight
        );

        Cube(
            glm::ivec3(
                x - 1,
                y + height + 4,
                z - 1
            ),
            glm::ivec3(
                x + 2,
                y + height + 8,
                z + 2
            ),
            leaf
        );
    };


    // ============================================================
    // HOUSE
    // ============================================================

    auto House =
        [&](int x,
            int y,
            int z,
            int width,
            int depth)
    {
        constexpr int WALL_HEIGHT = 13;

        Cube(
            glm::ivec3(
                x,
                y,
                z
            ),
            glm::ivec3(
                x + width,
                y + 2,
                z + depth
            ),
            stone
        );

        Cube(
            glm::ivec3(
                x + 2,
                y + 2,
                z + 2
            ),
            glm::ivec3(
                x + width - 2,
                y + WALL_HEIGHT,
                z + depth - 2
            ),
            brick
        );

        Cube(
            glm::ivec3(
                x + 4,
                y + 4,
                z + 4
            ),
            glm::ivec3(
                x + width - 4,
                y + WALL_HEIGHT - 2,
                z + depth - 4
            ),
            woodDark
        );

        Cube(
            glm::ivec3(
                x + 2,
                y + 2,
                z + depth - 3
            ),
            glm::ivec3(
                x + width - 2,
                y + WALL_HEIGHT,
                z + depth
            ),
            brick
        );

        Cube(
            glm::ivec3(
                x - 2,
                y + WALL_HEIGHT,
                z - 2
            ),
            glm::ivec3(
                x + width + 2,
                y + WALL_HEIGHT + 4,
                z + depth + 2
            ),
            roof
        );

        Cube(
            glm::ivec3(
                x + 3,
                y + WALL_HEIGHT + 4,
                z + depth / 2 - 2
            ),
            glm::ivec3(
                x + width - 3,
                y + WALL_HEIGHT + 6,
                z + depth / 2 + 2
            ),
            roof
        );

        Cube(
            glm::ivec3(
                x + width / 2 - 2,
                y + 3,
                z + depth - 4
            ),
            glm::ivec3(
                x + width / 2 + 2,
                y + 10,
                z + depth
            ),
            woodDark
        );

        Cube(
            glm::ivec3(
                x + 4,
                y + 7,
                z + depth - 1
            ),
            glm::ivec3(
                x + 10,
                y + 11,
                z + depth + 1
            ),
            glass
        );

        Cube(
            glm::ivec3(
                x + width - 10,
                y + 7,
                z + depth - 1
            ),
            glm::ivec3(
                x + width - 4,
                y + 11,
                z + depth + 1
            ),
            glass
        );

        Cube(
            glm::ivec3(
                x - 1,
                y + 7,
                z + depth / 2 - 3
            ),
            glm::ivec3(
                x + 2,
                y + 11,
                z + depth / 2 + 3
            ),
            glass
        );
    };


    // ============================================================
    // LARGE HOUSE
    // ============================================================

    auto LargeHouse =
        [&](int x,
            int y,
            int z)
    {
        constexpr int W = 34;
        constexpr int D = 30;
        constexpr int H = 18;

        Cube(
            glm::ivec3(x, y, z),
            glm::ivec3(
                x + W,
                y + 3,
                z + D
            ),
            stone
        );

        Cube(
            glm::ivec3(
                x + 2,
                y + 3,
                z + 2
            ),
            glm::ivec3(
                x + W - 2,
                y + H,
                z + D - 2
            ),
            brick
        );

        Cube(
            glm::ivec3(
                x + 5,
                y + 5,
                z + 5
            ),
            glm::ivec3(
                x + W - 5,
                y + H - 2,
                z + D - 5
            ),
            woodDark
        );

        Cube(
            glm::ivec3(
                x - 3,
                y + H,
                z - 3
            ),
            glm::ivec3(
                x + W + 3,
                y + H + 5,
                z + D + 3
            ),
            roof
        );

        Cube(
            glm::ivec3(
                x + W / 2 - 3,
                y + 3,
                z + D - 4
            ),
            glm::ivec3(
                x + W / 2 + 3,
                y + 12,
                z + D
            ),
            woodDark
        );

        Cube(
            glm::ivec3(
                x + 5,
                y + 8,
                z + D - 1
            ),
            glm::ivec3(
                x + 12,
                y + 13,
                z + D + 1
            ),
            glass
        );

        Cube(
            glm::ivec3(
                x + W - 12,
                y + 8,
                z + D - 1
            ),
            glm::ivec3(
                x + W - 5,
                y + 13,
                z + D + 1
            ),
            glass
        );
    };


    // ============================================================
    // WATCHTOWER
    // ============================================================

    auto Watchtower =
        [&](int x,
            int y,
            int z)
    {
        constexpr int W = 14;
        constexpr int H = 38;

        Cube(
            glm::ivec3(x, y, z),
            glm::ivec3(
                x + W,
                y + H,
                z + W
            ),
            stoneDark
        );

        Cube(
            glm::ivec3(
                x + 3,
                y + 3,
                z + 3
            ),
            glm::ivec3(
                x + W - 3,
                y + H - 3,
                z + W - 3
            ),
            stone
        );

        for (int wy = 10;
             wy < H - 5;
             wy += 10)
        {
            Cube(
                glm::ivec3(
                    x + W / 2 - 2,
                    y + wy,
                    z - 1
                ),
                glm::ivec3(
                    x + W / 2 + 2,
                    y + wy + 5,
                    z + 2
                ),
                glass
            );
        }

        for (int i = 0; i < 3; ++i)
        {
            Cube(
                glm::ivec3(
                    x + i * 5,
                    y + H,
                    z
                ),
                glm::ivec3(
                    x + i * 5 + 3,
                    y + H + 5,
                    z + 4
                ),
                stoneDark
            );

            Cube(
                glm::ivec3(
                    x + i * 5,
                    y + H,
                    z + W - 4
                ),
                glm::ivec3(
                    x + i * 5 + 3,
                    y + H + 5,
                    z + W
                ),
                stoneDark
            );
        }
    };


    // ============================================================
    // RUIN
    // ============================================================

    auto Ruin =
        [&](int x,
            int y,
            int z)
    {
        int h1 = 14;
        int h2 = 22;
        int h3 = 11;
        int h4 = 18;

        Cube(
            glm::ivec3(
                x,
                y,
                z
            ),
            glm::ivec3(
                x + 7,
                y + h1,
                z + 7
            ),
            stoneDark
        );

        Cube(
            glm::ivec3(
                x + 10,
                y,
                z
            ),
            glm::ivec3(
                x + 16,
                y + h2,
                z + 6
            ),
            stone
        );

        Cube(
            glm::ivec3(
                x,
                y,
                z + 11
            ),
            glm::ivec3(
                x + 6,
                y + h3,
                z + 17
            ),
            stone
        );

        Cube(
            glm::ivec3(
                x + 11,
                y,
                z + 12
            ),
            glm::ivec3(
                x + 17,
                y + h4,
                z + 18
            ),
            stoneDark
        );

        Cube(
            glm::ivec3(
                x + 5,
                y,
                z + 5
            ),
            glm::ivec3(
                x + 13,
                y + 2,
                z + 13
            ),
            stoneLight
        );
    };


    // ============================================================
    // SHRINE
    // ============================================================

    auto Shrine =
        [&](int x,
            int y,
            int z)
    {
        Cube(
            glm::ivec3(
                x - 14,
                y,
                z - 14
            ),
            glm::ivec3(
                x + 14,
                y + 3,
                z + 14
            ),
            stone
        );

        Cube(
            glm::ivec3(
                x - 8,
                y + 3,
                z - 5
            ),
            glm::ivec3(
                x + 8,
                y + 6,
                z + 5
            ),
            stoneLight
        );

        const glm::ivec2 pillars[] =
        {
            {-10, -10},
            { 10, -10},
            {-10,  10},
            { 10,  10}
        };

        for (const auto& p : pillars)
        {
            Cube(
                glm::ivec3(
                    x + p.x - 2,
                    y + 3,
                    z + p.y - 2
                ),
                glm::ivec3(
                    x + p.x + 2,
                    y + 18,
                    z + p.y + 2
                ),
                stoneDark
            );
        }

        Cube(
            glm::ivec3(
                x - 5,
                y + 6,
                z - 5
            ),
            glm::ivec3(
                x + 5,
                y + 11,
                z + 5
            ),
            stoneDark
        );

        Cube(
            glm::ivec3(
                x - 2,
                y + 11,
                z - 2
            ),
            glm::ivec3(
                x + 2,
                y + 14,
                z + 2
            ),
            gold
        );
    };


    // ============================================================
    // BRIDGE
    // ============================================================

    auto Bridge =
        [&](int x,
            int y,
            int z,
            int length)
    {
        Cube(
            glm::ivec3(
                x,
                y,
                z
            ),
            glm::ivec3(
                x + length,
                y + 4,
                z + 10
            ),
            wood
        );

        for (int px = x;
             px <= x + length;
             px += 8)
        {
            Cube(
                glm::ivec3(
                    px,
                    y - 5,
                    z
                ),
                glm::ivec3(
                    px + 3,
                    y,
                    z + 3
                ),
                woodDark
            );

            Cube(
                glm::ivec3(
                    px,
                    y - 5,
                    z + 7
                ),
                glm::ivec3(
                    px + 3,
                    y,
                    z + 10
                ),
                woodDark
            );
        }
    };


    // ============================================================
    // GENERATE TERRAIN
    //
    // We generate 4x4 terrain patches.
    //
    // The height at each patch is obtained from the smooth
    // interpolated terrain function.
    //
    // This is considerably cheaper than 1x1 generation while
    // producing much smoother terrain than the original 8x8
    // blocks.
    // ============================================================

    std::vector<int> heightMap(
        WORLD_X * WORLD_Z
    );

    auto HeightIndex =
        [&](int x, int z)
    {
        return z * WORLD_X + x;
    };


    for (int x = 0;
         x < WORLD_X;
         x += TERRAIN_STEP)
    {
        for (int z = 0;
             z < WORLD_Z;
             z += TERRAIN_STEP)
        {
            int x1 =
                std::min(
                    x + TERRAIN_STEP,
                    WORLD_X
                );

            int z1 =
                std::min(
                    z + TERRAIN_STEP,
                    WORLD_Z
                );


            int width =
                x1 - x;

            int depth =
                z1 - z;


            // Sample the interpolated height at the center
            // of the patch.
            int centerX =
                x + width / 2;

            int centerZ =
                z + depth / 2;

            int height =
                GetHeight(
                    centerX,
                    centerZ
                );


            Biome biome =
                GetBiome(
                    centerX,
                    centerZ,
                    height
                );


            Voxel surface =
                grass;

            Voxel subsurface =
                dirt;


            if (biome == Biome::Desert)
            {
                surface =
                    sand;

                subsurface =
                    sandDark;
            }
            else if (biome == Biome::Mountain)
            {
                surface =
                    height > 130
                        ? snow
                        : stone;

                subsurface =
                    stone;
            }
            else if (biome == Biome::Swamp)
            {
                surface =
                    grassDark;

                subsurface =
                    dirtDark;
            }
            else if (biome == Biome::Forest)
            {
                surface =
                    grass;

                subsurface =
                    dirt;
            }


            // ----------------------------------------------------
            // Store interpolated heights.
            //
            // These are also used later by structures and water.
            // ----------------------------------------------------

            for (int px = x;
                 px < x1;
                 ++px)
            {
                for (int pz = z;
                     pz < z1;
                     ++pz)
                {
                    heightMap[
                        HeightIndex(
                            px,
                            pz
                        )
                    ] =
                        GetHeight(
                            px,
                            pz
                        );
                }
            }


            // ----------------------------------------------------
            // Deep terrain
            // ----------------------------------------------------

            Cube(
                glm::ivec3(
                    x,
                    0,
                    z
                ),
                glm::ivec3(
                    x1,
                    std::max(
                        1,
                        height - 8
                    ),
                    z1
                ),
                stoneDark
            );


            // ----------------------------------------------------
            // Stone
            // ----------------------------------------------------

            if (height > 10)
            {
                Cube(
                    glm::ivec3(
                        x,
                        10,
                        z
                    ),
                    glm::ivec3(
                        x1,
                        height - 5,
                        z1
                    ),
                    stone
                );
            }


            // ----------------------------------------------------
            // Dirt
            // ----------------------------------------------------

            if (height > 5)
            {
                Cube(
                    glm::ivec3(
                        x,
                        height - 5,
                        z
                    ),
                    glm::ivec3(
                        x1,
                        height - 1,
                        z1
                    ),
                    subsurface
                );
            }


            // ----------------------------------------------------
            // Surface
            // ----------------------------------------------------

            Cube(
                glm::ivec3(
                    x,
                    height - 1,
                    z
                ),
                glm::ivec3(
                    x1,
                    height + 1,
                    z1
                ),
                surface
            );
        }
    }


    // ============================================================
    // WATER
    // ============================================================

    for (int x = 0;
         x < WORLD_X;
         x += TERRAIN_STEP)
    {
        for (int z = 0;
             z < WORLD_Z;
             z += TERRAIN_STEP)
        {
            int x1 =
                std::min(
                    x + TERRAIN_STEP,
                    WORLD_X
                );

            int z1 =
                std::min(
                    z + TERRAIN_STEP,
                    WORLD_Z
                );


            int centerX =
                x + (x1 - x) / 2;

            int centerZ =
                z + (z1 - z) / 2;


            int terrainY =
                heightMap[
                    HeightIndex(
                        centerX,
                        centerZ
                    )
                ];


            if (terrainY >= SEA_LEVEL)
                continue;


            Biome biome =
                GetBiome(
                    centerX,
                    centerZ,
                    terrainY
                );


            if (biome == Biome::Desert)
                continue;


            Cube(
                glm::ivec3(
                    x,
                    terrainY + 1,
                    z
                ),
                glm::ivec3(
                    x1,
                    SEA_LEVEL,
                    z1
                ),
                water
            );
        }
    }


    // ============================================================
    // STRUCTURE SITE SEARCH
    // ============================================================

    auto FindStructureSite =
        [&](int startX,
            int startZ,
            int radius,
            int step,
            int footprintX,
            int footprintZ,
            int maxSlope,
            int minY,
            int maxY,
            bool allowDesert,
            bool allowMountain,
            glm::ivec2& result) -> bool
    {
        int bestScore =
            -1000000000;

        int bestX = 0;
        int bestZ = 0;


        for (int z = startZ - radius;
             z <= startZ + radius;
             z += step)
        {
            for (int x = startX - radius;
                 x <= startX + radius;
                 x += step)
            {
                if (x < footprintX + 8 ||
                    z < footprintZ + 8 ||
                    x >= WORLD_X - footprintX - 8 ||
                    z >= WORLD_Z - footprintZ - 8)
                {
                    continue;
                }


                int centerY =
                    GetHeight(
                        x,
                        z
                    );


                if (centerY < minY ||
                    centerY > maxY)
                {
                    continue;
                }


                Biome biome =
                    GetBiome(
                        x,
                        z,
                        centerY
                    );


                if (!allowDesert &&
                    biome == Biome::Desert)
                {
                    continue;
                }


                if (!allowMountain &&
                    biome == Biome::Mountain)
                {
                    continue;
                }


                if (biome == Biome::Swamp)
                    continue;


                int minHeight =
                    100000;

                int maxHeight =
                    -100000;


                const int samples = 5;


                for (int sy = 0;
                     sy < samples;
                     ++sy)
                {
                    for (int sx = 0;
                         sx < samples;
                         ++sx)
                    {
                        int px =
                            x -
                            footprintX / 2 +
                            (footprintX * sx) /
                            (samples - 1);

                        int pz =
                            z -
                            footprintZ / 2 +
                            (footprintZ * sy) /
                            (samples - 1);


                        int py =
                            GetHeight(
                                px,
                                pz
                            );


                        minHeight =
                            std::min(
                                minHeight,
                                py
                            );

                        maxHeight =
                            std::max(
                                maxHeight,
                                py
                            );
                    }
                }


                int slope =
                    maxHeight -
                    minHeight;


                if (slope > maxSlope)
                    continue;


                int distance =
                    std::abs(
                        x - startX
                    ) +
                    std::abs(
                        z - startZ
                    );


                int score =
                    10000 -
                    slope * 100 -
                    distance;


                if (score > bestScore)
                {
                    bestScore =
                        score;

                    bestX =
                        x;

                    bestZ =
                        z;
                }
            }
        }


        if (bestScore ==
            -1000000000)
        {
            return false;
        }


        result =
            glm::ivec2(
                bestX,
                bestZ
            );


        return true;
    };


    // ============================================================
    // TREE GENERATION
    // ============================================================

    std::mt19937 rng(
        839271
    );

    std::uniform_int_distribution<int>
        xDistribution(
            12,
            WORLD_X - 13
        );

    std::uniform_int_distribution<int>
        zDistribution(
            12,
            WORLD_Z - 13
        );

    std::uniform_real_distribution<float>
        random01(
            0.0f,
            1.0f
        );


    std::vector<glm::ivec2>
        placedTrees;


    constexpr int TREE_CANDIDATES =
        (WORLD_X * WORLD_Z) / 850;


    for (int i = 0;
         i < TREE_CANDIDATES;
         ++i)
    {
        int x =
            xDistribution(rng);

        int z =
            zDistribution(rng);

        int y =
            GetHeight(
                x,
                z
            );


        if (y <= SEA_LEVEL + 5)
            continue;


        Biome biome =
            GetBiome(
                x,
                z,
                y
            );


        float density;


        switch (biome)
        {
            case Biome::Forest:
                density = 0.72f;
                break;

            case Biome::Plains:
                density = 0.10f;
                break;

            case Biome::Swamp:
                density = 0.35f;
                break;

            case Biome::Desert:
                density = 0.008f;
                break;

            case Biome::Mountain:
                density = 0.015f;
                break;
        }


        float patch =
            treeNoise.GetNoise(
                static_cast<float>(x),
                static_cast<float>(z)
            );


        patch =
            patch * 0.5f +
            0.5f;


        density *=
            0.3f +
            patch * 1.2f;


        if (random01(rng) >
            glm::clamp(
                density,
                0.0f,
                0.95f
            ))
        {
            continue;
        }


        int h0 =
            GetHeight(
                x,
                z
            );

        int h1 =
            GetHeight(
                x + 6,
                z
            );

        int h2 =
            GetHeight(
                x - 6,
                z
            );

        int h3 =
            GetHeight(
                x,
                z + 6
            );

        int h4 =
            GetHeight(
                x,
                z - 6
            );


        int slope =
            std::max(
                {
                    std::abs(h0 - h1),
                    std::abs(h0 - h2),
                    std::abs(h0 - h3),
                    std::abs(h0 - h4)
                }
            );


        if (slope > 8)
            continue;


        bool tooClose =
            false;


        int minimumDistance =
            biome == Biome::Forest
                ? 12
                : 18;


        for (const auto& p :
             placedTrees)
        {
            int dx =
                x - p.x;

            int dz =
                z - p.y;


            if (dx * dx +
                dz * dz <
                minimumDistance *
                minimumDistance)
            {
                tooClose = true;
                break;
            }
        }


        if (tooClose)
            continue;


        int treeHeight =
            7 +
            static_cast<int>(
                random01(rng) * 7.0f
            );


        if (biome ==
            Biome::Swamp)
        {
            treeHeight += 3;
        }


        Tree(
            x,
            y + 1,
            z,
            treeHeight,
            biome
        );


        placedTrees.emplace_back(
            x,
            z
        );
    }


    // ============================================================
    // VILLAGES
    // ============================================================

    for (int gx = 180;
         gx < WORLD_X - 180;
         gx += 600)
    {
        for (int gz = 180;
             gz < WORLD_Z - 180;
             gz += 600)
        {
            float n =
                structureNoise.GetNoise(
                    static_cast<float>(gx),
                    static_cast<float>(gz)
                );


            if (n < -0.25f)
                continue;


            glm::ivec2 village;


            if (!FindStructureSite(
                    gx,
                    gz,
                    220,
                    12,
                    100,
                    100,
                    8,
                    SEA_LEVEL + 6,
                    105,
                    false,
                    false,
                    village))
            {
                continue;
            }


            int vx =
                village.x;

            int vz =
                village.y;

            int vy =
                GetHeight(
                    vx,
                    vz
                );


            // Plaza
            Cube(
                glm::ivec3(
                    vx - 14,
                    vy + 1,
                    vz - 14
                ),
                glm::ivec3(
                    vx + 14,
                    vy + 4,
                    vz + 14
                ),
                stone
            );


            // Main roads
            Cube(
                glm::ivec3(
                    vx - 70,
                    vy + 2,
                    vz - 3
                ),
                glm::ivec3(
                    vx + 70,
                    vy + 4,
                    vz + 3
                ),
                road
            );


            Cube(
                glm::ivec3(
                    vx - 3,
                    vy + 2,
                    vz - 70
                ),
                glm::ivec3(
                    vx + 3,
                    vy + 4,
                    vz + 70
                ),
                road
            );


            // Houses
            const glm::ivec2
                houseOffsets[] =
            {
                {-50, -42},
                { 25, -42},
                {-50,  28},
                { 25,  28}
            };


            for (const auto& offset :
                 houseOffsets)
            {
                glm::ivec2 site;


                if (!FindStructureSite(
                        vx + offset.x,
                        vz + offset.y,
                        45,
                        5,
                        27,
                        25,
                        5,
                        SEA_LEVEL + 5,
                        105,
                        false,
                        false,
                        site))
                {
                    continue;
                }


                int hy =
                    GetHeight(
                        site.x,
                        site.y
                    );


                House(
                    site.x - 13,
                    hy + 1,
                    site.y - 12,
                    26,
                    24
                );
            }


            // Large house
            glm::ivec2 hall;


            if (FindStructureSite(
                    vx,
                    vz + 80,
                    50,
                    5,
                    36,
                    32,
                    5,
                    SEA_LEVEL + 5,
                    105,
                    false,
                    false,
                    hall))
            {
                int hy =
                    GetHeight(
                        hall.x,
                        hall.y
                    );


                LargeHouse(
                    hall.x - 18,
                    hy + 1,
                    hall.y - 16
                );
            }


            // Watchtower
            glm::ivec2 towerSite;


            if (FindStructureSite(
                    vx + 75,
                    vz + 75,
                    50,
                    6,
                    18,
                    18,
                    7,
                    SEA_LEVEL + 5,
                    115,
                    false,
                    false,
                    towerSite))
            {
                int ty =
                    GetHeight(
                        towerSite.x,
                        towerSite.y
                    );


                Watchtower(
                    towerSite.x - 7,
                    ty + 1,
                    towerSite.y - 7
                );
            }


            // Fountain
            Cube(
                glm::ivec3(
                    vx - 7,
                    vy + 4,
                    vz - 7
                ),
                glm::ivec3(
                    vx + 7,
                    vy + 6,
                    vz + 7
                ),
                stoneDark
            );


            Cube(
                glm::ivec3(
                    vx - 3,
                    vy + 6,
                    vz - 3
                ),
                glm::ivec3(
                    vx + 3,
                    vy + 8,
                    vz + 3
                ),
                water
            );
        }
    }


    // ============================================================
    // RUINS
    // ============================================================

    for (int gx = 140;
         gx < WORLD_X - 140;
         gx += 360)
    {
        for (int gz = 140;
             gz < WORLD_Z - 140;
             gz += 360)
        {
            float n =
                structureNoise.GetNoise(
                    static_cast<float>(gx + 1000),
                    static_cast<float>(gz + 1000)
                );


            if (n < -0.05f)
                continue;


            glm::ivec2 site;


            if (!FindStructureSite(
                    gx,
                    gz,
                    150,
                    10,
                    24,
                    24,
                    10,
                    SEA_LEVEL + 5,
                    125,
                    true,
                    false,
                    site))
            {
                continue;
            }


            int y =
                GetHeight(
                    site.x,
                    site.y
                );


            Ruin(
                site.x - 9,
                y + 1,
                site.y - 9
            );
        }
    }


    // ============================================================
    // MOUNTAIN SHRINES
    // ============================================================

    for (int gx = 220;
         gx < WORLD_X - 220;
         gx += 500)
    {
        for (int gz = 220;
             gz < WORLD_Z - 220;
             gz += 500)
        {
            glm::ivec2 site;


            if (!FindStructureSite(
                    gx,
                    gz,
                    200,
                    10,
                    30,
                    30,
                    12,
                    105,
                    WORLD_Y - 30,
                    false,
                    true,
                    site))
            {
                continue;
            }


            int y =
                GetHeight(
                    site.x,
                    site.y
                );


            if (GetBiome(
                    site.x,
                    site.y,
                    y
                ) != Biome::Mountain)
            {
                continue;
            }


            Shrine(
                site.x,
                y + 1,
                site.y
            );
        }
    }


    // ============================================================
    // DESERT RUINS
    // ============================================================

    for (int gx = 160;
         gx < WORLD_X - 160;
         gx += 420)
    {
        for (int gz = 160;
             gz < WORLD_Z - 160;
             gz += 420)
        {
            glm::ivec2 site;


            if (!FindStructureSite(
                    gx,
                    gz,
                    170,
                    10,
                    26,
                    26,
                    8,
                    SEA_LEVEL + 5,
                    100,
                    true,
                    false,
                    site))
            {
                continue;
            }


            int y =
                GetHeight(
                    site.x,
                    site.y
                );


            if (GetBiome(
                    site.x,
                    site.y,
                    y
                ) != Biome::Desert)
            {
                continue;
            }


            Cube(
                glm::ivec3(
                    site.x - 10,
                    y + 1,
                    site.y - 10
                ),
                glm::ivec3(
                    site.x - 3,
                    y + 15,
                    site.y - 3
                ),
                sandDark
            );


            Cube(
                glm::ivec3(
                    site.x + 3,
                    y + 1,
                    site.y - 10
                ),
                glm::ivec3(
                    site.x + 10,
                    y + 11,
                    site.y - 3
                ),
                sand
            );


            Cube(
                glm::ivec3(
                    site.x - 10,
                    y + 1,
                    site.y + 3
                ),
                glm::ivec3(
                    site.x - 3,
                    y + 18,
                    site.y + 10
                ),
                sand
            );


            Cube(
                glm::ivec3(
                    site.x + 3,
                    y + 1,
                    site.y + 3
                ),
                glm::ivec3(
                    site.x + 10,
                    y + 13,
                    site.y + 10
                ),
                sandDark
            );


            Cube(
                glm::ivec3(
                    site.x - 5,
                    y + 1,
                    site.y - 5
                ),
                glm::ivec3(
                    site.x + 5,
                    y + 3,
                    site.y + 5
                ),
                sand
            );
        }
    }


    // ============================================================
    // BRIDGES
    // ============================================================

    for (int gx = 200;
         gx < WORLD_X - 200;
         gx += 500)
    {
        for (int gz = 300;
             gz < WORLD_Z - 300;
             gz += 700)
        {
            int y =
                GetHeight(
                    gx,
                    gz
                );


            if (y > SEA_LEVEL + 12)
                continue;


            Bridge(
                gx,
                SEA_LEVEL + 2,
                gz,
                80
            );
        }
    }


    // ============================================================
    // GRASS / VEGETATION PATCHES
    // ============================================================

    for (int x = 10;
         x < WORLD_X - 10;
         x += 18)
    {
        for (int z = 10;
             z < WORLD_Z - 10;
             z += 21)
        {
            int y =
                GetHeight(
                    x,
                    z
                );


            if (y <= SEA_LEVEL + 3)
                continue;


            Biome biome =
                GetBiome(
                    x,
                    z,
                    y
                );


            if (biome == Biome::Desert ||
                biome == Biome::Mountain)
            {
                continue;
            }


            float n =
                detailNoise.GetNoise(
                    static_cast<float>(x),
                    static_cast<float>(z)
                );


            if (n < 0.15f)
                continue;


            Cube(
                glm::ivec3(
                    x,
                    y + 1,
                    z
                ),
                glm::ivec3(
                    x + 4,
                    y + 3,
                    z + 4
                ),
                grassDark
            );
        }
    }


    // ============================================================
    // TORCHES AROUND VILLAGES
    // ============================================================

    for (int gx = 180;
         gx < WORLD_X - 180;
         gx += 600)
    {
        for (int gz = 180;
             gz < WORLD_Z - 180;
             gz += 600)
        {
            float n =
                structureNoise.GetNoise(
                    static_cast<float>(gx),
                    static_cast<float>(gz)
                );


            if (n < -0.25f)
                continue;


            glm::ivec2 site;


            if (!FindStructureSite(
                    gx,
                    gz,
                    220,
                    12,
                    100,
                    100,
                    8,
                    SEA_LEVEL + 6,
                    105,
                    false,
                    false,
                    site))
            {
                continue;
            }


            for (int dx = -60;
                 dx <= 60;
                 dx += 20)
            {
                int tx =
                    site.x + dx;

                int tz =
                    site.y + 18;

                int ty =
                    GetHeight(
                        tx,
                        tz
                    );


                Cube(
                    glm::ivec3(
                        tx,
                        ty + 1,
                        tz
                    ),
                    glm::ivec3(
                        tx + 2,
                        ty + 7,
                        tz + 2
                    ),
                    woodDark
                );


                Cube(
                    glm::ivec3(
                        tx - 1,
                        ty + 7,
                        tz - 1
                    ),
                    glm::ivec3(
                        tx + 3,
                        ty + 10,
                        tz + 3
                    ),
                    torch
                );
            }
        }
    }


    // ============================================================
    // TEST STRUCTURE
    // ============================================================

    Voxel v;

    v.set_solid(true);
    v.set_r(31);
    v.set_g(31);
    v.set_b(31);

    vm.FillVoxels(
        glm::ivec3(
            512,
            0,
            256
        ),
        glm::ivec3(
            542,
            1000,
            270
        ),
        v
    );
}

void GenerateCaves(VoxelManager& vm)
{
    for (int x = 0; x < 4; x++)
        for (int y = 0; y < 4; y++)
            for (int z = 0; z < 4; z++)
                vm.AllocateChunk(glm::ivec3(x, y, z));

    vm.GenerateChunkOccupancyMap();

    constexpr int CHUNK_SIZE = 64;
    constexpr float AIR_THRESHOLD = 5.0f;

    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetSeed(static_cast<int>(std::time(nullptr)));
    noise.SetFrequency(0.02f);

    // ------------------------------------------------------------
    // Materials
    // ------------------------------------------------------------

    auto voxel = [](uint8_t r, uint8_t g, uint8_t b, bool solid = true)
    {
        Voxel v{};
        v.set_r(r);
        v.set_g(g);
        v.set_b(b);
        v.set_solid(solid);
        return v;
    };

    Voxel stone = voxel(10, 10, 10);
    Voxel air   = voxel(0, 0, 0, false);

    // ------------------------------------------------------------
    // Noise
    // ------------------------------------------------------------

    auto calcNoise = [&](float gx, float gy, float gz) -> float
    {
        float n = noise.GetNoise(gx, gy, gz) * 20.0f;

        n -= sinf(gx * 0.1f + gz * 0.2f) * 10.0f;
        n -= cosf(gy * 0.08f + gx * 0.15f) * 8.0f;

        return n;
    };

    // ------------------------------------------------------------
    // Process chunks
    // ------------------------------------------------------------

    for (int ci = 0; ci < vm.allocated_chunks.size(); ci++)
    {
        Chunk chunk = vm.allocated_chunks[ci];

        const glm::ivec3 worldOrigin =
            chunk.position * CHUNK_SIZE;

        // --------------------------------------------------------
        // IMPORTANT:
        //
        // These are persistent buffers.
        // They MUST exist outside the Z loop.
        // --------------------------------------------------------

        float slicePrevStorage[64][64];
        float sliceCurrStorage[64][64];
        float sliceNextStorage[64][64];

        float (*slicePrev)[64] = slicePrevStorage;
        float (*sliceCurr)[64] = sliceCurrStorage;
        float (*sliceNext)[64] = sliceNextStorage;

        // --------------------------------------------------------
        // Initial Z = 0 and Z = 1
        //
        // This is exactly what the original does.
        // --------------------------------------------------------

        for (int y = 0; y < CHUNK_SIZE; y++)
        {
            for (int x = 0; x < CHUNK_SIZE; x++)
            {
                float gx = x + chunk.position.x * 64;
                float gy = y + chunk.position.y * 64;

                float gz = 0 + chunk.position.z * 64;

                sliceCurr[x][y] =
                    calcNoise(gx, gy, gz);
            }
        }

        for (int y = 0; y < CHUNK_SIZE; y++)
        {
            for (int x = 0; x < CHUNK_SIZE; x++)
            {
                float gx = x + chunk.position.x * 64;
                float gy = y + chunk.position.y * 64;

                float gz = 1 + chunk.position.z * 64;

                sliceNext[x][y] =
                    calcNoise(gx, gy, gz);
            }
        }

        // --------------------------------------------------------
        // Traverse Z
        // --------------------------------------------------------

        for (int z = 0; z < CHUNK_SIZE; z++)
        {
            float (*slice_below)[64] = slicePrev;
            float (*slice_curr_ptr)[64] = sliceCurr;
            float (*slice_above)[64] = sliceNext;

            for (int y = 0; y < CHUNK_SIZE; y++)
            {
                for (int x = 0; x < CHUNK_SIZE; x++)
                {
                    float gx =
                        x + chunk.position.x * 64;

                    float gy =
                        y + chunk.position.y * 64;

                    float gz =
                        z + chunk.position.z * 64;

                    float dC =
                        slice_curr_ptr[x][y];

                    // ------------------------------------------------
                    // AIR
                    // ------------------------------------------------

                    if (dC < AIR_THRESHOLD)
                    {
                        vm.SetVoxel(
                            glm::ivec3(gx, gy, gz),
                            air
                        );

                        continue;
                    }

                    // ------------------------------------------------
                    // 6-neighbor samples
                    // ------------------------------------------------

                    float dXP =
                        (x < 63)
                            ? slice_curr_ptr[x + 1][y]
                            : calcNoise(
                                x + chunk.position.x * 64 + 1,
                                y + chunk.position.y * 64,
                                z + chunk.position.z * 64
                            );

                    float dXN =
                        (x > 0)
                            ? slice_curr_ptr[x - 1][y]
                            : calcNoise(
                                x + chunk.position.x * 64 - 1,
                                y + chunk.position.y * 64,
                                z + chunk.position.z * 64
                            );

                    float dYP =
                        (y < 63)
                            ? slice_curr_ptr[x][y + 1]
                            : calcNoise(
                                x + chunk.position.x * 64,
                                y + chunk.position.y * 64 + 1,
                                z + chunk.position.z * 64
                            );

                    float dYN =
                        (y > 0)
                            ? slice_curr_ptr[x][y - 1]
                            : calcNoise(
                                x + chunk.position.x * 64,
                                y + chunk.position.y * 64 - 1,
                                z + chunk.position.z * 64
                            );

                    float dZP =
                        (z < 63)
                            ? slice_above[x][y]
                            : calcNoise(
                                x + chunk.position.x * 64,
                                y + chunk.position.y * 64,
                                z + chunk.position.z * 64 + 1
                            );

                    float dZN =
                        (z > 0)
                            ? slice_below[x][y]
                            : calcNoise(
                                x + chunk.position.x * 64,
                                y + chunk.position.y * 64,
                                z + chunk.position.z * 64 - 1
                            );

                    // ------------------------------------------------
                    // Completely surrounded
                    // ------------------------------------------------

                    if (dXP < AIR_THRESHOLD &&
                        dXN < AIR_THRESHOLD &&
                        dYP < AIR_THRESHOLD &&
                        dYN < AIR_THRESHOLD &&
                        dZP < AIR_THRESHOLD &&
                        dZN < AIR_THRESHOLD)
                    {
                        vm.SetVoxel(
                            glm::ivec3(gx, gy, gz),
                            stone
                        );

                        continue;
                    }

                    // ------------------------------------------------
                    // Surface normal
                    // ------------------------------------------------

                    float nx = dXP - dXN;
                    float ny = dYP - dYN;
                    float nz = dZP - dZN;

                    float nl =
                        sqrtf(
                            nx * nx +
                            ny * ny +
                            nz * nz
                        ) + 1e-6f;

                    nx /= nl;
                    ny /= nl;
                    nz /= nl;

                    // ------------------------------------------------
                    // Base stone color
                    // ------------------------------------------------

                    int R = 10 + rand() % 2;
                    int G = 10 + rand() % 1;
                    int B = 10 + rand() % 6;

                    // ------------------------------------------------
                    // Grass
                    // ------------------------------------------------

                    if (-ny > 0.5f)
                    {
                        float blend =
                            (-ny - 0.5f) / 0.5f;

                        int green_base =
                            22 + (rand() % 6);

                        int green_high =
                            28 + (rand() % 4);

                        float g_noise =
                            noise.GetNoise(
                                gx * 0.2f,
                                gy * 0.2f,
                                gz * 0.2f
                            );

                        g_noise =
                            (g_noise + 1.0f) * 0.5f;

                        int green =
                            static_cast<int>(
                                green_base * (1.0f - g_noise) +
                                green_high * g_noise
                            );

                        R = static_cast<int>(
                            R * (1.0f - blend) +
                            green * blend
                        );

                        G = static_cast<int>(
                            G * (1.0f - blend) +
                            6 * blend
                        );

                        B = static_cast<int>(
                            B * (1.0f - blend) +
                            6 * blend
                        );

                        int highlight =
                            rand() % 3;

                        G =
                            (G + highlight > 31)
                                ? 31
                                : G + highlight;
                    }

                    Voxel v = voxel(
                        static_cast<uint8_t>(R),
                        static_cast<uint8_t>(G),
                        static_cast<uint8_t>(B)
                    );

                    vm.SetVoxel(
                        glm::ivec3(gx, gy, gz),
                        v
                    );
                }
            }

            // --------------------------------------------------------
            // EXACT ORIGINAL SLICE SWAP
            // --------------------------------------------------------

            float (*tmp)[64] = slicePrev;

            slicePrev = sliceCurr;
            sliceCurr = sliceNext;
            sliceNext = tmp;

            // --------------------------------------------------------
            // Generate z + 2
            // --------------------------------------------------------

            int znext = z + 2;

            if (znext < 64)
            {
                for (int y = 0; y < 64; y++)
                {
                    for (int x = 0; x < 64; x++)
                    {
                        float gx =
                            x + chunk.position.x * 64;

                        float gy =
                            y + chunk.position.y * 64;

                        float gz =
                            znext + chunk.position.z * 64;

                        sliceNext[x][y] =
                            calcNoise(gx, gy, gz);
                    }
                }
            }
        }
    }
}

}