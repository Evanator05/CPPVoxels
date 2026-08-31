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
    constexpr int CHUNK_SIZE = 64;

    constexpr int CHUNKS_X = 16;
    constexpr int CHUNKS_Y = 3;
    constexpr int CHUNKS_Z = 16;

    constexpr int WORLD_X = CHUNKS_X * CHUNK_SIZE;
    constexpr int WORLD_Y = CHUNKS_Y * CHUNK_SIZE;
    constexpr int WORLD_Z = CHUNKS_Z * CHUNK_SIZE;

    constexpr int SEA_LEVEL = 40;

    // Sample terrain every 4 voxels.
    // This keeps generation reasonably cheap while still giving
    // smooth terrain through interpolation.
    constexpr int TERRAIN_STEP = 4;

    printf("\n=== GENERATING WORLD ===\n");


    // ============================================================
    // ALLOCATE CHUNKS
    // ============================================================

    const int totalChunks =
        CHUNKS_X * CHUNKS_Y * CHUNKS_Z;

    int allocatedChunks = 0;

    printf(
        "Allocating %d x %d x %d chunks...\n",
        CHUNKS_X,
        CHUNKS_Y,
        CHUNKS_Z
    );

    for (int x = 0; x < CHUNKS_X; ++x)
    {
        for (int y = 0; y < CHUNKS_Y; ++y)
        {
            for (int z = 0; z < CHUNKS_Z; ++z)
            {
                vm.AllocateChunk(glm::ivec3(x, y, z));

                ++allocatedChunks;

                if (allocatedChunks % 100 == 0 ||
                    allocatedChunks == totalChunks)
                {
                    printf(
                        "  Chunks: %d / %d (%.1f%%)\n",
                        allocatedChunks,
                        totalChunks,
                        allocatedChunks * 100.0f / totalChunks
                    );
                }
            }
        }
    }

    printf("Generating chunk occupancy map...\n");

    vm.GenerateChunkOccupancyMap();

    const glm::ivec3 worldOrigin =
        vm.chunk_occupancy.position * CHUNK_SIZE;

    printf(
        "World origin: %d, %d, %d\n",
        worldOrigin.x,
        worldOrigin.y,
        worldOrigin.z
    );


    // ============================================================
    // MATERIALS
    // ============================================================

    auto MakeVoxel =
        [](uint8_t r, uint8_t g, uint8_t b)
    {
        Voxel v{};

        v.set_r(r);
        v.set_g(g);
        v.set_b(b);
        v.set_solid(true);

        return v;
    };

    const Voxel stone     = MakeVoxel(14, 14, 15);
    const Voxel stoneDark = MakeVoxel(9, 9, 10);
    const Voxel dirt      = MakeVoxel(16, 10, 5);
    const Voxel grass     = MakeVoxel(8, 24, 5);
    const Voxel snow      = MakeVoxel(27, 28, 29);
    const Voxel wood      = MakeVoxel(18, 10, 4);
    const Voxel leaves    = MakeVoxel(5, 20, 6);
    const Voxel water     = MakeVoxel(3, 12, 28);


    // ============================================================
    // HASH
    // ============================================================

    auto Hash =
        [](uint32_t x)
    {
        x ^= x >> 16;
        x *= 0x7feb352dU;

        x ^= x >> 15;
        x *= 0x846ca68bU;

        x ^= x >> 16;

        return x;
    };


    // ============================================================
    // RANDOMIZED VOXEL
    //
    // Randomizes each individual voxel rather than each cube.
    // ============================================================

    auto RandomizeVoxel =
        [&](Voxel voxel,
            int x,
            int y,
            int z,
            uint32_t seed)
    {
        uint32_t h =
            seed ^
            (static_cast<uint32_t>(x) * 73856093U) ^
            (static_cast<uint32_t>(y) * 19349663U) ^
            (static_cast<uint32_t>(z) * 83492791U);

        h = Hash(h);

        // Small variation so the terrain isn't perfectly flat.
        const int variation =
            static_cast<int>(h % 5U) - 2;

        voxel.set_r(
            glm::clamp(
                static_cast<int>(voxel.r()) + variation,
                0,
                31
            )
        );

        voxel.set_g(
            glm::clamp(
                static_cast<int>(voxel.g()) + variation,
                0,
                31
            )
        );

        voxel.set_b(
            glm::clamp(
                static_cast<int>(voxel.b()) + variation,
                0,
                31
            )
        );

        return voxel;
    };


    // ============================================================
    // SET VOXEL
    //
    // Everything generated below is relative to
    // vm.chunk_occupancy.position.
    //
    // SetVoxel() receives world coordinates, so worldOrigin
    // converts our local [0..WORLD] coordinates into the actual
    // allocated chunk coordinates.
    // ============================================================

    auto Set =
        [&](int x, int y, int z, const Voxel& voxel, uint32_t seed)
    {
        if (x < 0 || x >= WORLD_X ||
            y < 0 || y >= WORLD_Y ||
            z < 0 || z >= WORLD_Z)
        {
            return;
        }

        vm.SetVoxel(
            glm::ivec3(x, y, z) + worldOrigin,
            RandomizeVoxel(voxel, x, y, z, seed)
        );
    };


    // ============================================================
    // FAST SOLID COLUMN
    //
    // Instead of calling FillVoxels once per voxel, terrain is
    // generated one column at a time with SetVoxel.
    //
    // The important optimization is that we only generate the
    // relatively thin surface region individually. Large interior
    // regions are filled with larger SetVoxel batches where
    // possible through the 64-voxel chunk hierarchy.
    // ============================================================

    auto SetColumn =
        [&](int x, int z, int bottom, int top)
    {
        bottom = std::max(bottom, 0);
        top = std::min(top, WORLD_Y);

        if (bottom >= top)
            return;

        for (int y = bottom; y < top; ++y)
        {
            Voxel voxel;

            if (y < top - 4)
                voxel = stone;
            else
                voxel = grass;

            Set(x, y, z, voxel, 1001);
        }
    };


    // ============================================================
    // NOISE
    // ============================================================

    printf("Setting up noise...\n");

    // Main terrain shape.
    FastNoiseLite terrainNoise;

    terrainNoise.SetNoiseType(
        FastNoiseLite::NoiseType_OpenSimplex2
    );

    terrainNoise.SetSeed(48291);
    terrainNoise.SetFrequency(0.0018f);


    // Medium scale terrain variation.
    FastNoiseLite detailNoise;

    detailNoise.SetNoiseType(
        FastNoiseLite::NoiseType_OpenSimplex2
    );

    detailNoise.SetSeed(192837);
    detailNoise.SetFrequency(0.008f);


    // Tree distribution.
    FastNoiseLite treeNoise;

    treeNoise.SetNoiseType(
        FastNoiseLite::NoiseType_OpenSimplex2
    );

    treeNoise.SetSeed(912837);
    treeNoise.SetFrequency(0.006f);


    // Rock distribution.
    FastNoiseLite rockNoise;

    rockNoise.SetNoiseType(
        FastNoiseLite::NoiseType_OpenSimplex2
    );

    rockNoise.SetSeed(712341);
    rockNoise.SetFrequency(0.012f);


    // ============================================================
    // HEIGHT SAMPLES
    // ============================================================

    constexpr int SAMPLE_X =
        WORLD_X / TERRAIN_STEP + 1;

    constexpr int SAMPLE_Z =
        WORLD_Z / TERRAIN_STEP + 1;

    std::vector<float> heightSamples(
        SAMPLE_X * SAMPLE_Z
    );

    auto SampleIndex =
        [&](int x, int z)
    {
        return z * SAMPLE_X + x;
    };


    printf("Generating terrain height samples...\n");

    for (int sz = 0; sz < SAMPLE_Z; ++sz)
    {
        for (int sx = 0; sx < SAMPLE_X; ++sx)
        {
            const float x =
                static_cast<float>(
                    std::min(
                        sx * TERRAIN_STEP,
                        WORLD_X - 1
                    )
                );

            const float z =
                static_cast<float>(
                    std::min(
                        sz * TERRAIN_STEP,
                        WORLD_Z - 1
                    )
                );


            // Main broad terrain.
            const float large =
                terrainNoise.GetNoise(x, z);


            // Smaller terrain features.
            const float detail =
                detailNoise.GetNoise(x, z);


            // ----------------------------------------------------
            // Terrain shape
            //
            // Keep the original terrain noise while adding a
            // moderate amount of sharper variation.
            // ----------------------------------------------------

            float ridge =
                1.0f - std::abs(large);

            ridge *= ridge;


            float height = 42.0f;

            height += large * 22.0f;

            height += detail * 8.0f;

            // Moderate mountain contribution.
            height += ridge * 55.0f;


            // Slightly exaggerate high/low areas without making
            // the terrain completely insane.
            if (large > 0.45f)
            {
                height +=
                    (large - 0.45f) * 25.0f;
            }

            if (large < -0.55f)
            {
                height +=
                    (large + 0.55f) * 10.0f;
            }


            heightSamples[
                SampleIndex(sx, sz)
            ] =
                glm::clamp(
                    height,
                    8.0f,
                    static_cast<float>(WORLD_Y - 20)
                );
        }

        if (sz % 32 == 0 ||
            sz == SAMPLE_Z - 1)
        {
            printf(
                "  Samples: %d / %d (%.1f%%)\n",
                sz + 1,
                SAMPLE_Z,
                (sz + 1) * 100.0f / SAMPLE_Z
            );
        }
    }


    // ============================================================
    // HEIGHT FUNCTION
    // ============================================================

    auto GetHeight =
        [&](int x, int z) -> int
    {
        x = glm::clamp(x, 0, WORLD_X - 1);
        z = glm::clamp(z, 0, WORLD_Z - 1);

        const int sx =
            x / TERRAIN_STEP;

        const int sz =
            z / TERRAIN_STEP;

        const int sx1 =
            std::min(sx + 1, SAMPLE_X - 1);

        const int sz1 =
            std::min(sz + 1, SAMPLE_Z - 1);


        float tx =
            static_cast<float>(
                x % TERRAIN_STEP
            ) / TERRAIN_STEP;

        float tz =
            static_cast<float>(
                z % TERRAIN_STEP
            ) / TERRAIN_STEP;


        // Smooth interpolation.
        tx = tx * tx * (3.0f - 2.0f * tx);
        tz = tz * tz * (3.0f - 2.0f * tz);


        const float h00 =
            heightSamples[
                SampleIndex(sx, sz)
            ];

        const float h10 =
            heightSamples[
                SampleIndex(sx1, sz)
            ];

        const float h01 =
            heightSamples[
                SampleIndex(sx, sz1)
            ];

        const float h11 =
            heightSamples[
                SampleIndex(sx1, sz1)
            ];


        const float h0 =
            glm::mix(h00, h10, tx);

        const float h1 =
            glm::mix(h01, h11, tx);


        return static_cast<int>(
            glm::mix(h0, h1, tz)
        );
    };


    // ============================================================
    // HEIGHT MAP
    //
    // Cached because trees, rocks and terrain all need the height.
    // ============================================================

    printf("Building height map...\n");

    std::vector<int> heightMap(
        WORLD_X * WORLD_Z
    );

    auto HeightIndex =
        [&](int x, int z)
    {
        return z * WORLD_X + x;
    };


    for (int x = 0; x < WORLD_X; ++x)
    {
        for (int z = 0; z < WORLD_Z; ++z)
        {
            heightMap[
                HeightIndex(x, z)
            ] =
                GetHeight(x, z);
        }

        if (x % 128 == 0 ||
            x == WORLD_X - 1)
        {
            printf(
                "  Height map: %d / %d (%.1f%%)\n",
                x + 1,
                WORLD_X,
                (x + 1) * 100.0f / WORLD_X
            );
        }
    }


    // ============================================================
    // TERRAIN
    //
    // Generate at full voxel resolution, but use the chunk
    // hierarchy naturally through SetVoxel's chunk lookup.
    //
    // Terrain is intentionally simple:
    //
    //   bottom       = dark stone
    //   upper stone  = normal stone
    //   surface      = dirt/grass
    //   high peaks   = snow
    //
    // This avoids the expensive collection of individual
    // FillVoxels calls from the previous version.
    // ============================================================

    printf("\nGenerating terrain...\n");

    for (int x = 0; x < WORLD_X; ++x)
    {
        for (int z = 0; z < WORLD_Z; ++z)
        {
            const int height =
                heightMap[
                    HeightIndex(x, z)
                ];


            // Generate the column.

            for (int y = 0; y < height; ++y)
            {
                Voxel voxel;

                if (y < 6)
                {
                    voxel = stoneDark;
                }
                else if (y < height - 3)
                {
                    voxel = stone;
                }
                else if (height >= 105)
                {
                    voxel = snow;
                }
                else if (height <= SEA_LEVEL)
                {
                    voxel = dirt;
                }
                else
                {
                    voxel = grass;
                }

                Set(
                    x,
                    y,
                    z,
                    voxel,
                    1000
                );
            }
        }

        if (x % 64 == 0 ||
            x == WORLD_X - 1)
        {
            printf(
                "  Terrain: %d / %d (%.1f%%)\n",
                x + 1,
                WORLD_X,
                (x + 1) * 100.0f / WORLD_X
            );
        }
    }


    // ============================================================
    // WATER
    // ============================================================

    printf("\nGenerating water...\n");

    for (int x = 0; x < WORLD_X; ++x)
    {
        for (int z = 0; z < WORLD_Z; ++z)
        {
            const int height =
                heightMap[
                    HeightIndex(x, z)
                ];


            if (height >= SEA_LEVEL)
                continue;


            for (int y = height;
                 y < SEA_LEVEL;
                 ++y)
            {
                Set(
                    x,
                    y,
                    z,
                    water,
                    1006
                );
            }
        }

        if (x % 128 == 0 ||
            x == WORLD_X - 1)
        {
            printf(
                "  Water: %d / %d (%.1f%%)\n",
                x + 1,
                WORLD_X,
                (x + 1) * 100.0f / WORLD_X
            );
        }
    }


    // ============================================================
// REALISTIC TREES + ROCKS
// ============================================================

printf("\nGenerating vegetation and rocks...\n");


// ------------------------------------------------------------
// Deterministic random
// ------------------------------------------------------------


auto Random01 = [&](int x, int y, int z, uint32_t seed)
{
    uint32_t h =
        Hash(
            seed ^
            static_cast<uint32_t>(x * 73856093) ^
            static_cast<uint32_t>(y * 19349663) ^
            static_cast<uint32_t>(z * 83492791)
        );

    return static_cast<float>(h) /
           static_cast<float>(UINT32_MAX);
};


// ------------------------------------------------------------
// Materials
// ------------------------------------------------------------


const Voxel leavesDark =
    MakeVoxel(3, 15, 4);

const Voxel rock =
    MakeVoxel(13, 13, 14);

const Voxel rockDark =
    MakeVoxel(8, 8, 9);


// ------------------------------------------------------------
// Per voxel material variation
// ------------------------------------------------------------



// ============================================================
// TREE GENERATOR
// ============================================================
//
// Trees are approximately:
//
//   45-80 voxels tall
//   3-5 voxel trunk
//   irregular branches
//   irregular crown
//   tapered trunk
//
// This is deliberately voxel-based rather than using large
// FillVoxels calls so the shape is actually irregular.
// ============================================================

auto GenerateTree =
    [&](int x,
        int groundY,
        int z,
        uint32_t seed)
{
    const float random =
        Random01(
            x,
            groundY,
            z,
            seed
        );

    // 45-80 voxel trees
    const int height =
        45 +
        static_cast<int>(
            random * 36.0f
        );

    // Larger trees get slightly thicker trunks.
    const int trunkRadius =
        1 +
        static_cast<int>(
            height / 35
        );

    const int top =
        groundY + height;


    // --------------------------------------------------------
    // Trunk
    // --------------------------------------------------------

    for (int y = groundY + 1;
         y < top;
         ++y)
    {
        // Slightly taper toward the top.
        int radius = trunkRadius;

        if (y > groundY + height * 0.65f)
            radius = std::max(1, radius - 1);

        // Slowly bend the trunk.
        const float t =
            static_cast<float>(
                y - groundY
            ) /
            static_cast<float>(
                height
            );

        const int bendX =
            static_cast<int>(
                std::sin(
                    t * 3.0f +
                    random * 6.28f
                ) *
                t *
                2.0f
            );

        const int bendZ =
            static_cast<int>(
                std::cos(
                    t * 2.5f +
                    random * 4.0f
                ) *
                t *
                2.0f
            );


        for (int dx = -radius;
             dx <= radius;
             ++dx)
        {
            for (int dz = -radius;
                 dz <= radius;
                 ++dz)
            {
                if (dx * dx + dz * dz >
                    radius * radius)
                {
                    continue;
                }

                Voxel v =
                    Random01(
                        x + bendX + dx,
                        y,
                        z + bendZ + dz,
                        seed + 100
                    ) < 0.25f
                    ? wood
                    : wood;

                v =
                    RandomizeVoxel(
                        v,
                        x + bendX + dx,
                        y,
                        z + bendZ + dz,
                        seed + 200
                    );

                vm.SetVoxel(
                    glm::ivec3(
                        x + bendX + dx,
                        y,
                        z + bendZ + dz
                    ) + worldOrigin,
                    v
                );
            }
        }
    }


    // --------------------------------------------------------
    // Branches
    // --------------------------------------------------------

    const int branchStart =
        groundY +
        height * 45 / 100;

    const int branchEnd =
        groundY +
        height * 82 / 100;

    const int branchCount =
        5 +
        static_cast<int>(
            random * 5.0f
        );


    for (int branch = 0;
         branch < branchCount;
         ++branch)
    {
        const float branchT =
            static_cast<float>(branch) /
            static_cast<float>(
                branchCount
            );

        const int branchY =
            branchStart +
            static_cast<int>(
                branchT *
                (branchEnd - branchStart)
            );

        const float angle =
            Random01(
                x,
                branchY,
                z,
                seed + branch * 73
            ) *
            6.2831853f;


        const int branchLength =
            7 +
            static_cast<int>(
                (1.0f - branchT) *
                10.0f
            );


        const int startX =
            x +
            static_cast<int>(
                std::sin(
                    branchT * 3.0f +
                    random * 6.28f
                ) *
                branchT *
                2.0f
            );

        const int startZ =
            z +
            static_cast<int>(
                std::cos(
                    branchT * 2.5f +
                    random * 4.0f
                ) *
                branchT *
                2.0f
            );


        for (int i = 0;
             i < branchLength;
             ++i)
        {
            const float t =
                static_cast<float>(i) /
                static_cast<float>(
                    branchLength
                );

            const int bx =
                startX +
                static_cast<int>(
                    std::cos(angle) *
                    i
                );

            const int bz =
                startZ +
                static_cast<int>(
                    std::sin(angle) *
                    i
                );

            const int by =
                branchY +
                static_cast<int>(
                    t * 5.0f
                );


            // Branch becomes thinner toward its end.
            const int radius =
                i < branchLength * 0.45f
                ? 1
                : 0;


            for (int dx = -radius;
                 dx <= radius;
                 ++dx)
            {
                for (int dz = -radius;
                     dz <= radius;
                     ++dz)
                {
                    if (dx * dx + dz * dz >
                        radius * radius)
                    {
                        continue;
                    }

                    Voxel v =
                        RandomizeVoxel(
                            wood,
                            bx + dx,
                            by,
                            bz + dz,
                            seed + 300
                        );

                    vm.SetVoxel(
                        glm::ivec3(
                            bx + dx,
                            by,
                            bz + dz
                        ) + worldOrigin,
                        v
                    );
                }
            }
        }
    }


    // --------------------------------------------------------
    // Crown
    // --------------------------------------------------------

    const int crownCenterY =
        groundY +
        height * 82 / 100;

    const int crownRadius =
        9 +
        static_cast<int>(
            random * 6.0f
        );


    for (int dy = -crownRadius;
         dy <= crownRadius;
         ++dy)
    {
        const float vertical =
            static_cast<float>(dy) /
            static_cast<float>(
                crownRadius
            );

        // Wider around the middle.
        const float layer =
            1.0f -
            vertical * vertical;

        const int radius =
            static_cast<int>(
                crownRadius *
                layer
            );


        for (int dx = -radius;
             dx <= radius;
             ++dx)
        {
            for (int dz = -radius;
                 dz <= radius;
                 ++dz)
            {
                const float distance =
                    std::sqrt(
                        static_cast<float>(
                            dx * dx +
                            dz * dz
                        )
                    );

                // Irregular edge.
                const float noise =
                    Random01(
                        x + dx,
                        crownCenterY + dy,
                        z + dz,
                        seed + 500
                    );

                const float allowed =
                    radius *
                    (0.78f +
                     noise * 0.35f);

                if (distance > allowed)
                    continue;


                // Don't make every interior voxel solid.
                if (noise < 0.10f &&
                    distance > radius * 0.45f)
                {
                    continue;
                }


                Voxel v =
                    noise < 0.30f
                    ? leavesDark
                    : leaves;


                v =
                    RandomizeVoxel(
                        v,
                        x + dx,
                        crownCenterY + dy,
                        z + dz,
                        seed + 600
                    );


                vm.SetVoxel(
                    glm::ivec3(
                        x + dx,
                        crownCenterY + dy,
                        z + dz
                    ) + worldOrigin,
                    v
                );
            }
        }
    }


    // --------------------------------------------------------
    // Smaller lower foliage clusters
    // --------------------------------------------------------

    for (int i = 0; i < 4; ++i)
    {
        const float angle =
            Random01(
                x,
                i,
                z,
                seed + 700
            ) *
            6.2831853f;

        const int distance =
            4 +
            static_cast<int>(
                Random01(
                    x,
                    i,
                    z,
                    seed + 701
                ) * 5.0f
            );

        const int cx =
            x +
            static_cast<int>(
                std::cos(angle) *
                distance
            );

        const int cz =
            z +
            static_cast<int>(
                std::sin(angle) *
                distance
            );

        const int cy =
            groundY +
            height * 65 / 100 +
            i * 2;


        for (int dx = -3;
             dx <= 3;
             ++dx)
        {
            for (int dz = -3;
                 dz <= 3;
                 ++dz)
            {
                const float d =
                    std::sqrt(
                        static_cast<float>(
                            dx * dx +
                            dz * dz
                        )
                    );

                if (d > 3.5f)
                    continue;

                const float n =
                    Random01(
                        cx + dx,
                        cy,
                        cz + dz,
                        seed + 800
                    );

                if (n < 0.15f)
                    continue;

                Voxel v =
                    n < 0.45f
                    ? leavesDark
                    : leaves;

                v =
                    RandomizeVoxel(
                        v,
                        cx + dx,
                        cy,
                        cz + dz,
                        seed + 801
                    );

                vm.SetVoxel(
                    glm::ivec3(
                        cx + dx,
                        cy,
                        cz + dz
                    ) + worldOrigin,
                    v
                );
            }
        }
    }
};


// ============================================================
// ROCK GENERATOR
// ============================================================
//
// Rocks are irregular 3D blobs rather than cubes.
// Typical size: 4-14 voxels.
// ============================================================

auto GenerateRock =
    [&](int x,
        int groundY,
        int z,
        uint32_t seed)
{
    const float r =
        Random01(
            x,
            groundY,
            z,
            seed
        );


    // 4-14 voxel radius
    const int radius =
        2 +
        static_cast<int>(
            r * 6.0f
        );


    const int height =
        std::max(
            2,
            static_cast<int>(
                radius *
                (0.55f + r * 0.35f)
            )
        );


    for (int dy = 0;
         dy <= height;
         ++dy)
    {
        const float t =
            static_cast<float>(dy) /
            static_cast<float>(
                height
            );

        // Rocks become narrower toward the top.
        const float verticalScale =
            1.0f - t * 0.65f;

        const int layerRadius =
            std::max(
                1,
                static_cast<int>(
                    radius *
                    verticalScale
                )
            );


        for (int dx = -layerRadius;
             dx <= layerRadius;
             ++dx)
        {
            for (int dz = -layerRadius;
                 dz <= layerRadius;
                 ++dz)
            {
                const float distance =
                    std::sqrt(
                        static_cast<float>(
                            dx * dx +
                            dz * dz
                        )
                    );


                // Irregular surface.
                const float noise =
                    Random01(
                        x + dx,
                        groundY + dy,
                        z + dz,
                        seed + 100
                    );


                const float edge =
                    layerRadius *
                    (0.70f +
                     noise * 0.45f);


                if (distance > edge)
                    continue;


                // Remove some voxels from the edges.
                if (distance >
                    layerRadius * 0.65f &&
                    noise < 0.20f)
                {
                    continue;
                }


                Voxel v =
                    noise < 0.30f
                    ? rockDark
                    : rock;


                v =
                    RandomizeVoxel(
                        v,
                        x + dx,
                        groundY + dy,
                        z + dz,
                        seed + 200
                    );


                vm.SetVoxel(
                    glm::ivec3(
                        x + dx,
                        groundY + dy,
                        z + dz
                    ) + worldOrigin,
                    v
                );
            }
        }
    }
};


// ============================================================
// PLACE TREES
// ============================================================

printf("Generating trees...\n");

int treeCount = 0;

constexpr int TREE_SPACING = 48;

for (int x = 24;
     x < WORLD_X - 24;
     x += TREE_SPACING)
{
    for (int z = 24;
         z < WORLD_Z - 24;
         z += TREE_SPACING)
    {
        const float n =
            treeNoise.GetNoise(
                static_cast<float>(x),
                static_cast<float>(z)
            );


        // Sparse forest.
        if (n < 0.20f)
            continue;


        const int y =
            heightMap[
                HeightIndex(
                    x,
                    z
                )
            ];


        // Don't grow trees underwater
        // or on extreme mountains.
        if (y <= SEA_LEVEL + 2 ||
            y > 105)
        {
            continue;
        }


        // Avoid every tree looking like it
        // came from a perfect grid.
        const int ox =
            static_cast<int>(
                Random01(
                    x,
                    y,
                    z,
                    4001
                ) * 18.0f
            ) - 9;

        const int oz =
            static_cast<int>(
                Random01(
                    x,
                    y,
                    z,
                    4002
                ) * 18.0f
            ) - 9;


        GenerateTree(
            x + ox,
            y,
            z + oz,
            5000 + treeCount
        );

        ++treeCount;
    }


    if (x % 192 == 24)
    {
        printf(
            "  Trees: %d\n",
            treeCount
        );
    }
}


// ============================================================
// PLACE ROCKS
// ============================================================

printf("Generating rocks...\n");

int rockCount = 0;

constexpr int ROCK_SPACING = 24;

for (int x = 12;
     x < WORLD_X - 12;
     x += ROCK_SPACING)
{
    for (int z = 12;
         z < WORLD_Z - 12;
         z += ROCK_SPACING)
    {
        const float n =
            rockNoise.GetNoise(
                static_cast<float>(x),
                static_cast<float>(z)
            );


        if (n < 0.30f)
            continue;


        const int y =
            heightMap[
                HeightIndex(
                    x,
                    z
                )
            ];


        if (y <= SEA_LEVEL ||
            y > WORLD_Y - 20)
        {
            continue;
        }


        const int ox =
            static_cast<int>(
                Random01(
                    x,
                    y,
                    z,
                    6001
                ) * 10.0f
            ) - 5;

        const int oz =
            static_cast<int>(
                Random01(
                    x,
                    y,
                    z,
                    6002
                ) * 10.0f
            ) - 5;


        GenerateRock(
            x + ox,
            y,
            z + oz,
            7000 + rockCount
        );

        ++rockCount;
    }


    if (x % 192 == 12)
    {
        printf(
            "  Rocks: %d\n",
            rockCount
        );
    }
}


printf(
    "Trees generated: %d\n",
    treeCount
);

printf(
    "Rocks generated: %d\n",
    rockCount
);

    // ============================================================
    // COMPLETE
    // ============================================================

    printf(
        "\n=== WORLD GENERATION COMPLETE ===\n"
    );

    printf(
        "Chunks: %d\n",
        totalChunks
    );

    printf(
        "World: %d x %d x %d voxels\n",
        WORLD_X,
        WORLD_Y,
        WORLD_Z
    );

    printf(
        "Trees: %d\n",
        treeCount
    );

    printf(
        "Rocks: %d\n",
        rockCount
    );

    printf(
        "Origin: %d, %d, %d\n",
        worldOrigin.x,
        worldOrigin.y,
        worldOrigin.z
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


void GenerateFreaky(VoxelManager& vm)
{
    constexpr int CHUNK_SIZE = 64;

    constexpr int CHUNKS_X = 30;
    constexpr int CHUNKS_Y = 3;
    constexpr int CHUNKS_Z = 30;

    constexpr int WORLD_X = CHUNKS_X * CHUNK_SIZE;
    constexpr int WORLD_Y = CHUNKS_Y * CHUNK_SIZE;
    constexpr int WORLD_Z = CHUNKS_Z * CHUNK_SIZE;

    // Structure cells.
    constexpr int CELL = 16;

    // 64 / 16 = 4 cells per chunk.
    constexpr int CELLS_X = WORLD_X / CELL;
    constexpr int CELLS_Y = WORLD_Y / CELL;
    constexpr int CELLS_Z = WORLD_Z / CELL;

    constexpr uint32_t SEED = 1337;


    // ============================================================
    // MATERIALS
    // ============================================================

    auto MakeVoxel =
        [](uint8_t r, uint8_t g, uint8_t b)
    {
        Voxel v{};

        v.set_r(r);
        v.set_g(g);
        v.set_b(b);
        v.set_solid(true);

        return v;
    };

    const Voxel stone =
        MakeVoxel(13, 14, 15);

    const Voxel stoneDark =
        MakeVoxel(8, 9, 10);

    const Voxel metal =
        MakeVoxel(18, 19, 20);

    const Voxel metalDark =
        MakeVoxel(10, 11, 12);

    const Voxel glow =
        MakeVoxel(28, 22, 8);

    const Voxel black =
        MakeVoxel(2, 2, 3);


    // ============================================================
    // FAST HASH
    // ============================================================

    auto Hash =
        [](uint32_t x) -> uint32_t
    {
        x ^= x >> 16;
        x *= 0x7feb352dU;

        x ^= x >> 15;
        x *= 0x846ca68bU;

        x ^= x >> 16;

        return x;
    };


    auto Hash3 =
        [&](int x, int y, int z, uint32_t salt) -> uint32_t
    {
        uint32_t h =
            static_cast<uint32_t>(x) * 374761393U;

        h +=
            static_cast<uint32_t>(y) * 668265263U;

        h +=
            static_cast<uint32_t>(z) * 2147483647U;

        h +=
            salt * 2246822519U;

        h +=
            SEED * 3266489917U;

        return Hash(h);
    };


    auto Hash01 =
        [&](int x, int y, int z, uint32_t salt) -> float
    {
        return static_cast<float>(
            Hash3(x, y, z, salt)
        ) / 4294967295.0f;
    };


    auto HashChance =
        [&](int x, int y, int z,
            uint32_t salt,
            float chance) -> bool
    {
        return Hash01(x, y, z, salt) < chance;
    };


    // ============================================================
    // VOXEL COLOR RANDOMIZATION
    //
    // This happens once when a voxel is generated.
    // ============================================================

    auto Randomize =
        [&](Voxel voxel,
            int x,
            int y,
            int z,
            uint32_t salt)
    {
        const uint32_t h =
            Hash3(x, y, z, salt);

        // -2 ... +2
        const int variation =
            static_cast<int>(h % 5U) - 2;

        voxel.set_r(
            static_cast<uint8_t>(
                std::clamp(
                    static_cast<int>(voxel.r()) +
                        variation,
                    0,
                    31
                )
            )
        );

        voxel.set_g(
            static_cast<uint8_t>(
                std::clamp(
                    static_cast<int>(voxel.g()) +
                        variation,
                    0,
                    31
                )
            )
        );

        voxel.set_b(
            static_cast<uint8_t>(
                std::clamp(
                    static_cast<int>(voxel.b()) +
                        variation,
                    0,
                    31
                )
            )
        );

        return voxel;
    };


    // ============================================================
    // ALLOCATE CHUNKS
    // ============================================================

    printf("\n=== GENERATING MEGASTRUCTURE ===\n");

    printf(
        "Allocating %d x %d x %d chunks...\n",
        CHUNKS_X,
        CHUNKS_Y,
        CHUNKS_Z
    );

    const int totalChunks =
        CHUNKS_X *
        CHUNKS_Y *
        CHUNKS_Z;

    int allocated = 0;

    for (int x = 0; x < CHUNKS_X; ++x)
    {
        for (int y = 0; y < CHUNKS_Y; ++y)
        {
            for (int z = 0; z < CHUNKS_Z; ++z)
            {
                vm.AllocateChunk(
                    glm::ivec3(x, y, z)
                );

                ++allocated;
            }

            printf(
                "  Chunks: %d / %d (%.1f%%)\n",
                allocated,
                totalChunks,
                allocated * 100.0f /
                    totalChunks
            );
        }
    }


    // ============================================================
    // BUILD OCCUPANCY
    // ============================================================

    printf("Generating chunk occupancy map...\n");

    vm.GenerateChunkOccupancyMap();


    // ============================================================
    // WORLD ORIGIN
    //
    // Everything below is generated in local voxel coordinates
    // starting at 0,0,0.
    //
    // SetVoxel receives:
    //
    //     localPosition + worldOrigin
    //
    // so the generator works regardless of where the occupancy
    // tree happens to be located.
    // ============================================================

    const glm::ivec3 worldOrigin =
        vm.chunk_occupancy.position *
        CHUNK_SIZE;

    printf(
        "Occupancy origin: %d %d %d\n",
        vm.chunk_occupancy.position.x,
        vm.chunk_occupancy.position.y,
        vm.chunk_occupancy.position.z
    );

    printf(
        "Voxel origin: %d %d %d\n",
        worldOrigin.x,
        worldOrigin.y,
        worldOrigin.z
    );


    // ============================================================
    // SET VOXEL HELPER
    // ============================================================

    auto Set =
        [&](int x, int y, int z,
            const Voxel& voxel,
            uint32_t randomSalt)
    {
        if (x < 0 || x >= WORLD_X ||
            y < 0 || y >= WORLD_Y ||
            z < 0 || z >= WORLD_Z)
        {
            return;
        }

        vm.SetVoxel(
            worldOrigin +
                glm::ivec3(x, y, z),

            Randomize(
                voxel,
                x,
                y,
                z,
                randomSalt
            )
        );
    };


    // ============================================================
    // CELL STRUCTURE TYPES
    //
    // This mirrors the original generator:
    //
    // 0 open
    // 1 stacked floors
    // 2 girder lattice
    // 3 solid mass
    // 4 catwalk
    // 5 dense slabs
    // 6 stairs
    // 7 pipes
    // 8 rubble
    // 9 ziggurat
    // 10 shell
    // ============================================================

    enum StructureType
    {
        OPEN,
        FLOORS,
        LATTICE,
        MASS,
        CATWALK,
        SLABS,
        STAIRS,
        PIPES,
        RUBBLE,
        ZIGGURAT,
        SHELL
    };


    auto GetStructureType =
        [&](int cx, int cy, int cz)
        -> StructureType
    {
        const float t =
            Hash01(
                cx,
                cy,
                cz,
                7
            );

        if (t < 0.12f)
            return OPEN;

        if (t < 0.26f)
            return FLOORS;

        if (t < 0.39f)
            return LATTICE;

        if (t < 0.46f)
            return MASS;

        if (t < 0.55f)
            return CATWALK;

        if (t < 0.61f)
            return SLABS;

        if (t < 0.71f)
            return STAIRS;

        if (t < 0.80f)
            return PIPES;

        if (t < 0.85f)
            return RUBBLE;

        if (t < 0.91f)
            return ZIGGURAT;

        return SHELL;
    };


    // ============================================================
    // CELL VOXEL GENERATOR
    // ============================================================

    auto GenerateCell =
        [&](int cx, int cy, int cz)
    {
        const int baseX =
            cx * CELL;

        const int baseY =
            cy * CELL;

        const int baseZ =
            cz * CELL;


        const StructureType type =
            GetStructureType(
                cx,
                cy,
                cz
            );


        // --------------------------------------------------------
        // OCCASIONAL PILLARS
        // --------------------------------------------------------

        const bool pillar =
            (HashChance(
                cx / 3,
                cy,
                cz / 3,
                100,
                0.72f
            ));


        if (pillar)
        {
            constexpr int PILLAR = 2;

            for (int ly = 0;
                 ly < CELL;
                 ++ly)
            {
                const bool glowBand =
                    (ly % 4) == 0;

                for (int lx = 0;
                     lx < CELL;
                     ++lx)
                {
                    const bool left =
                        lx < PILLAR;

                    const bool right =
                        lx >= CELL - PILLAR;

                    if (!left && !right)
                        continue;

                    for (int lz = 0;
                         lz < CELL;
                         ++lz)
                    {
                        const bool front =
                            lz < PILLAR;

                        const bool back =
                            lz >= CELL - PILLAR;

                        if (!front && !back)
                            continue;

                        Set(
                            baseX + lx,
                            baseY + ly,
                            baseZ + lz,

                            glowBand
                                ? glow
                                : metal,

                            glowBand
                                ? 300
                                : 301
                        );
                    }
                }
            }
        }


        // --------------------------------------------------------
        // OPEN
        // --------------------------------------------------------

        if (type == OPEN)
            return;


        // --------------------------------------------------------
        // STACKED FLOORS
        // --------------------------------------------------------

        if (type == FLOORS)
        {
            for (int ly = 0;
                 ly < CELL;
                 ++ly)
            {
                if (ly >= 2 &&
                    ly < CELL - 2)
                    continue;

                for (int lx = 0;
                     lx < CELL;
                     ++lx)
                {
                    for (int lz = 0;
                         lz < CELL;
                         ++lz)
                    {
                        Set(
                            baseX + lx,
                            baseY + ly,
                            baseZ + lz,

                            metal,

                            100
                        );
                    }
                }
            }

            return;
        }


        // --------------------------------------------------------
        // GIRDER LATTICE
        // --------------------------------------------------------

        if (type == LATTICE)
        {
            for (int ly = 0;
                 ly < CELL;
                 ++ly)
            {
                for (int lx = 0;
                     lx < CELL;
                     ++lx)
                {
                    for (int lz = 0;
                         lz < CELL;
                         ++lz)
                    {
                        if ((ly % 3) < 2 ||
                            (lx % 4) < 2 ||
                            (lz % 4) < 2)
                        {
                            Set(
                                baseX + lx,
                                baseY + ly,
                                baseZ + lz,

                                metalDark,

                                101
                            );
                        }
                    }
                }
            }

            return;
        }


        // --------------------------------------------------------
        // SOLID MASS
        // --------------------------------------------------------

        if (type == MASS)
        {
            for (int ly = 0;
                 ly < CELL;
                 ++ly)
            {
                for (int lx = 0;
                     lx < CELL;
                     ++lx)
                {
                    for (int lz = 0;
                         lz < CELL;
                         ++lz)
                    {
                        Set(
                            baseX + lx,
                            baseY + ly,
                            baseZ + lz,

                            stone,

                            102
                        );
                    }
                }
            }

            return;
        }


        // --------------------------------------------------------
        // CATWALK
        // --------------------------------------------------------

        if (type == CATWALK)
        {
            constexpr int mid =
                CELL / 2;

            for (int ly = mid - 1;
                 ly <= mid + 1;
                 ++ly)
            {
                for (int lx = 0;
                     lx < CELL;
                     ++lx)
                {
                    for (int lz = 0;
                         lz < CELL;
                         ++lz)
                    {
                        Set(
                            baseX + lx,
                            baseY + ly,
                            baseZ + lz,

                            metal,

                            103
                        );
                    }
                }
            }

            return;
        }


        // --------------------------------------------------------
        // DENSE SLABS
        // --------------------------------------------------------

        if (type == SLABS)
        {
            for (int ly = 0;
                 ly < CELL;
                 ++ly)
            {
                for (int lx = 0;
                     lx < CELL;
                     ++lx)
                {
                    for (int lz = 0;
                         lz < CELL;
                         ++lz)
                    {
                        const bool slab =
                            (lx % 3) >= 1 &&
                            (lz % 3) >= 1;

                        const bool shelf =
                            (ly % 4) == 0;

                        if (!slab && !shelf)
                            continue;

                        Set(
                            baseX + lx,
                            baseY + ly,
                            baseZ + lz,

                            shelf
                                ? metal
                                : stone,

                            104
                        );
                    }
                }
            }

            return;
        }


        // --------------------------------------------------------
        // STAIRS
        // --------------------------------------------------------

        if (type == STAIRS)
        {
            constexpr int STEP_W = 2;
            constexpr int STEP_H = 2;

            for (int ly = 0;
                 ly < CELL;
                 ++ly)
            {
                const int step =
                    ly / STEP_H;

                const int treadX =
                    (step * STEP_W) %
                    CELL;

                for (int lx = 0;
                     lx < CELL;
                     ++lx)
                {
                    for (int lz = 0;
                         lz < CELL;
                         ++lz)
                    {
                        bool solid = false;

                        // Tread.
                        if ((ly % STEP_H) == 0 &&
                            lx >= treadX &&
                            lx < treadX + STEP_W + 1)
                        {
                            solid = true;
                        }

                        // Riser.
                        if (lx == treadX)
                            solid = true;

                        // Side walls.
                        if ((lz == 0 ||
                             lz == CELL - 1) &&
                            lx >= treadX - 1 &&
                            lx < treadX + STEP_W + 1)
                        {
                            solid = true;
                        }

                        if (!solid)
                            continue;

                        Set(
                            baseX + lx,
                            baseY + ly,
                            baseZ + lz,

                            metal,

                            105
                        );
                    }
                }
            }

            return;
        }


        // --------------------------------------------------------
        // PIPE BUNDLE
        // --------------------------------------------------------

        if (type == PIPES)
        {
            constexpr int pipeX[3] =
            {
                4, 11, 7
            };

            constexpr int pipeZ[3] =
            {
                4, 4, 11
            };

            constexpr int radius[3] =
            {
                2, 2, 3
            };

            for (int ly = 0;
                 ly < CELL;
                 ++ly)
            {
                for (int lx = 0;
                     lx < CELL;
                     ++lx)
                {
                    for (int lz = 0;
                         lz < CELL;
                         ++lz)
                    {
                        bool solid = false;
                        bool isGlow = false;

                        for (int p = 0;
                             p < 3;
                             ++p)
                        {
                            const int dx =
                                lx - pipeX[p];

                            const int dz =
                                lz - pipeZ[p];

                            const int d2 =
                                dx * dx +
                                dz * dz;

                            const int r =
                                radius[p];

                            if (d2 <= r * r &&
                                d2 >=
                                    (r - 1) *
                                    (r - 1))
                            {
                                solid = true;
                            }

                            if (d2 <=
                                    (r - 2) *
                                    (r - 2) &&
                                ly % 4 == 0)
                            {
                                isGlow = true;
                            }
                        }

                        // Pipe flanges.
                        if (ly % 5 == 0)
                        {
                            for (int p = 0;
                                 p < 3;
                                 ++p)
                            {
                                const int dx =
                                    std::abs(
                                        lx -
                                        pipeX[p]
                                    );

                                const int dz =
                                    std::abs(
                                        lz -
                                        pipeZ[p]
                                    );

                                if (dx <= radius[p] + 1 &&
                                    dz <= radius[p] + 1)
                                {
                                    solid = true;
                                }
                            }
                        }

                        if (!solid &&
                            !isGlow)
                            continue;

                        Set(
                            baseX + lx,
                            baseY + ly,
                            baseZ + lz,

                            isGlow
                                ? glow
                                : stone,

                            106
                        );
                    }
                }
            }

            return;
        }


        // --------------------------------------------------------
        // RUBBLE
        // --------------------------------------------------------

        if (type == RUBBLE)
        {
            for (int ly = 0;
                 ly < CELL;
                 ++ly)
            {
                const float height =
                    static_cast<float>(ly) /
                    static_cast<float>(CELL);

                const float fill =
                    0.85f -
                    height * 0.70f;

                for (int lx = 0;
                     lx < CELL;
                     ++lx)
                {
                    for (int lz = 0;
                         lz < CELL;
                         ++lz)
                    {
                        if (Hash01(
                                baseX + lx,
                                baseY + ly,
                                baseZ + lz,
                                33
                            ) >= fill)
                        {
                            continue;
                        }

                        const float material =
                            Hash01(
                                baseX + lx,
                                baseY + ly,
                                baseZ + lz,
                                34
                            );

                        const Voxel* voxel;

                        if (material < 0.07f)
                            voxel = &glow;
                        else if (material < 0.30f)
                            voxel = &metal;
                        else
                            voxel = &stone;

                        Set(
                            baseX + lx,
                            baseY + ly,
                            baseZ + lz,

                            *voxel,

                            107
                        );
                    }
                }
            }

            return;
        }


        // --------------------------------------------------------
        // ZIGGURAT
        // --------------------------------------------------------

        if (type == ZIGGURAT)
        {
            for (int ly = 0;
                 ly < CELL;
                 ++ly)
            {
                const int band =
                    (CELL - 1 - ly) / 2;

                const int min =
                    band;

                const int max =
                    CELL - 1 - band;

                for (int lx = 0;
                     lx < CELL;
                     ++lx)
                {
                    for (int lz = 0;
                         lz < CELL;
                         ++lz)
                    {
                        if (lx < min ||
                            lx > max ||
                            lz < min ||
                            lz > max)
                        {
                            Set(
                                baseX + lx,
                                baseY + ly,
                                baseZ + lz,

                                stone,

                                108
                            );

                            continue;
                        }

                        if (lx == min ||
                            lx == max ||
                            lz == min ||
                            lz == max)
                        {
                            Set(
                                baseX + lx,
                                baseY + ly,
                                baseZ + lz,

                                metal,

                                109
                            );
                        }
                    }
                }
            }

            return;
        }


        // --------------------------------------------------------
        // SHELL
        // --------------------------------------------------------

        if (type == SHELL)
        {
            constexpr int shell = 1;

            for (int ly = 0;
                 ly < CELL;
                 ++ly)
            {
                for (int lx = 0;
                     lx < CELL;
                     ++lx)
                {
                    for (int lz = 0;
                         lz < CELL;
                         ++lz)
                    {
                        if (lx >= shell &&
                            lx < CELL - shell &&
                            ly >= shell &&
                            ly < CELL - shell &&
                            lz >= shell &&
                            lz < CELL - shell)
                        {
                            continue;
                        }

                        Set(
                            baseX + lx,
                            baseY + ly,
                            baseZ + lz,

                            metal,

                            110
                        );
                    }
                }
            }

            return;
        }
    };


    // ============================================================
    // GENERATE CELLS
    //
    // 4 x 4 x 4 cells per 64 voxel chunk.
    //
    // This is only:
    //
    // 120 x 12 x 120 = 172,800 cells
    //
    // instead of treating the world as a collection of
    // FillVoxels operations.
    // ============================================================

    printf(
        "\nGenerating %d x %d x %d structure cells...\n",
        CELLS_X,
        CELLS_Y,
        CELLS_Z
    );

    const int totalCells =
        CELLS_X *
        CELLS_Y *
        CELLS_Z;

    int generatedCells = 0;


    for (int cy = 0;
         cy < CELLS_Y;
         ++cy)
    {
        for (int cx = 0;
             cx < CELLS_X;
             ++cx)
        {
            for (int cz = 0;
                 cz < CELLS_Z;
                 ++cz)
            {
                GenerateCell(
                    cx,
                    cy,
                    cz
                );

                ++generatedCells;
            }
        }

        printf(
            "  Cells: %d / %d (%.1f%%)\n",
            generatedCells,
            totalCells,
            generatedCells * 100.0f /
                totalCells
        );
    }


    // ============================================================
    // ADD LARGE-SCALE VERTICAL SHAFTS
    //
    // These are deliberately sparse so they don't dominate
    // generation time.
    // ============================================================

    printf("\nGenerating vertical shafts...\n");

    int shafts = 0;

    for (int x = 32;
         x < WORLD_X;
         x += 96)
    {
        for (int z = 32;
             z < WORLD_Z;
             z += 96)
        {
            if (!HashChance(
                    x / 96,
                    0,
                    z / 96,
                    500,
                    0.35f
                ))
            {
                continue;
            }

            ++shafts;

            const int radius =
                2 +
                static_cast<int>(
                    Hash01(
                        x,
                        0,
                        z,
                        501
                    ) * 3.0f
                );

            for (int y = 0;
                 y < WORLD_Y;
                 ++y)
            {
                for (int dx = -radius;
                     dx <= radius;
                     ++dx)
                {
                    for (int dz = -radius;
                         dz <= radius;
                         ++dz)
                    {
                        const int d2 =
                            dx * dx +
                            dz * dz;

                        if (d2 >
                            radius * radius)
                        {
                            continue;
                        }

                        Set(
                            x + dx,
                            y,
                            z + dz,

                            (y % 8 == 0)
                                ? glow
                                : metalDark,

                            502
                        );
                    }
                }
            }
        }
    }

    printf(
        "  Shafts: %d\n",
        shafts
    );


    // ============================================================
    // DONE
    // ============================================================

    printf(
        "\n=== MEGASTRUCTURE COMPLETE ===\n"
    );

    printf(
        "Chunks: %d\n",
        totalChunks
    );

    printf(
        "Cells: %d\n",
        totalCells
    );

    printf(
        "World: %d x %d x %d voxels\n",
        WORLD_X,
        WORLD_Y,
        WORLD_Z
    );

    printf(
        "Occupancy origin: %d %d %d\n",
        vm.chunk_occupancy.position.x,
        vm.chunk_occupancy.position.y,
        vm.chunk_occupancy.position.z
    );
}

}