#include "voxelrenderer.h"
#include "modules/renderer/shaderpasses/imguipass.h"
#include "modules/renderer/shaderpasses/blitpass.h"
#include "window.h"
#include "deltatime.h"
#include "input.h"
#include "console.h"
#include "modules/voxel/voxelmanager.h"

#include "shaders/depth.h"
#include "shaders/upscale.h"
#include "shaders/primary.h"

#include <string>
#include <math.h>



void VoxelRenderer::Init() {
    Window &window = GetModule<Window>();
    Renderer &renderer = GetModule<Renderer>();

    device = renderer.GetDevice();
    
    Texture *display = renderer.CreateResource<Texture>();
    display->size = window.GetSize();
    display->usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    display->format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    display->Create();

    Texture *halfDepth = renderer.CreateResource<Texture>();
    halfDepth->size = window.GetSize() / 2;
    halfDepth->usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    halfDepth->format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
    halfDepth->Create();

    Texture *fullDepth = renderer.CreateResource<Texture>();
    fullDepth->size = window.GetSize();
    fullDepth->usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    fullDepth->format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
    fullDepth->Create();

    VoxelManager &vm = GetModule<VoxelManager>();
    // ============================================================
// 10x10x10 Minecraft-style test world
// World size: 640 x 640 x 640 voxels
// Chunk size: 64
// ============================================================

for (int x = 0; x < 10; x++)
    for (int y = 0; y < 10; y++)
        for (int z = 0; z < 10; z++)
            vm.AllocateChunk(glm::ivec3(x, y, z));

vm.GenerateChunkOccupancyMap();

auto voxel = [](uint8_t r, uint8_t g, uint8_t b) {
    Voxel v{};
    v.set_r(r);
    v.set_g(g);
    v.set_b(b);
    v.set_solid(true);
    return v;
};

// ------------------------------------------------------------
// Materials
// ------------------------------------------------------------

Voxel grass     = voxel(8, 24, 5);
Voxel grassDark = voxel(5, 18, 4);
Voxel dirt      = voxel(16, 10, 5);
Voxel stone     = voxel(14, 14, 15);
Voxel stoneDark = voxel(8, 8, 9);
Voxel sand      = voxel(27, 23, 13);
Voxel water     = voxel(3, 12, 28);

Voxel wood      = voxel(18, 10, 4);
Voxel woodDark  = voxel(11, 6, 3);
Voxel leaves    = voxel(5, 20, 6);
Voxel leaves2   = voxel(8, 27, 8);

Voxel roof      = voxel(24, 5, 4);
Voxel brick     = voxel(23, 16, 10);
Voxel glass     = voxel(8, 20, 28);

Voxel road      = voxel(13, 11, 8);
Voxel torch     = voxel(31, 20, 4);

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------

auto cube = [&](glm::ivec3 a, glm::ivec3 b, Voxel v) {
    vm.FillVoxels(a, b, v);
};

auto tree = [&](int x, int y, int z, int height = 12) {

    // Trunk
    cube(
        glm::ivec3(x - 2, y, z - 2),
        glm::ivec3(x + 3, y + height, z + 3),
        wood
    );

    // Lower foliage
    cube(
        glm::ivec3(x - 8, y + height - 5, z - 8),
        glm::ivec3(x + 9, y + height + 3, z + 9),
        leaves
    );

    // Upper foliage
    cube(
        glm::ivec3(x - 5, y + height + 1, z - 5),
        glm::ivec3(x + 6, y + height + 8, z + 6),
        leaves2
    );

    // Crown
    cube(
        glm::ivec3(x - 2, y + height + 6, z - 2),
        glm::ivec3(x + 3, y + height + 11, z + 3),
        leaves
    );
};

auto house = [&](int x, int y, int z, int width, int depth) {

    int wallHeight = 18;

    // Foundation
    cube(
        glm::ivec3(x, y, z),
        glm::ivec3(x + width, y + 3, z + depth),
        stone
    );

    // Walls
    cube(
        glm::ivec3(x + 3, y + 3, z + 3),
        glm::ivec3(x + width - 3, y + wallHeight, z + depth - 3),
        brick
    );

    // Interior opening
    cube(
        glm::ivec3(x + 6, y + 6, z + 3),
        glm::ivec3(x + width - 6, y + wallHeight - 3, z + 5),
        wood
    );

    // Roof
    cube(
        glm::ivec3(x - 3, y + wallHeight, z - 3),
        glm::ivec3(x + width + 3, y + wallHeight + 5, z + depth + 3),
        roof
    );

    // Roof ridge
    cube(
        glm::ivec3(x + 4, y + wallHeight + 5, z + depth / 2 - 3),
        glm::ivec3(x + width - 4, y + wallHeight + 8, z + depth / 2 + 3),
        roof
    );

    // Door
    cube(
        glm::ivec3(x + width / 2 - 3, y + 4, z),
        glm::ivec3(x + width / 2 + 3, y + 12, z + 4),
        woodDark
    );

    // Windows
    cube(
        glm::ivec3(x + 6, y + 10, z - 1),
        glm::ivec3(x + 15, y + 15, z + 2),
        glass
    );

    cube(
        glm::ivec3(x + width - 15, y + 10, z - 1),
        glm::ivec3(x + width - 6, y + 15, z + 2),
        glass
    );

    cube(
        glm::ivec3(x - 1, y + 10, z + depth / 2 - 4),
        glm::ivec3(x + 2, y + 15, z + depth / 2 + 4),
        glass
    );
};

auto tower = [&](int x, int y, int z) {

    int width = 24;
    int height = 70;

    cube(
        glm::ivec3(x, y, z),
        glm::ivec3(x + width, y + height, z + width),
        stoneDark
    );

    // Inner tower
    cube(
        glm::ivec3(x + 5, y + 5, z + 5),
        glm::ivec3(x + width - 5, y + height - 5, z + width - 5),
        stone
    );

    // Battlements
    for (int i = 0; i < 4; i++) {
        cube(
            glm::ivec3(
                x + i * 6,
                y + height,
                z
            ),
            glm::ivec3(
                x + i * 6 + 4,
                y + height + 8,
                z + 6
            ),
            stoneDark
        );

        cube(
            glm::ivec3(
                x + i * 6,
                y + height,
                z + width - 6
            ),
            glm::ivec3(
                x + i * 6 + 4,
                y + height + 8,
                z + width
            ),
            stoneDark
        );
    }

    // Windows
    for (int wy = 20; wy < height - 10; wy += 18) {
        cube(
            glm::ivec3(x + width / 2 - 3, y + wy, z - 1),
            glm::ivec3(x + width / 2 + 3, y + wy + 8, z + 2),
            glass
        );
    }
};

// ============================================================
// TERRAIN
// ============================================================

// Large base
cube(
    glm::ivec3(0, 0, 0),
    glm::ivec3(640, 30, 640),
    stoneDark
);

// Broad rolling terrain.
// Large cubes are used so the scene doesn't require hundreds
// of thousands of FillVoxels calls.

for (int x = 0; x < 640; x += 16) {
    for (int z = 0; z < 640; z += 16) {

        float fx = (float)x;
        float fz = (float)z;

        // Rolling terrain
        float h =
            65.0f
            + sin(fx * 0.018f) * 22.0f
            + sin(fz * 0.021f) * 18.0f
            + sin((fx + fz) * 0.010f) * 25.0f
            + sin(fx * 0.047f + fz * 0.031f) * 8.0f;

        // Large mountain region
        float dx = fx - 480.0f;
        float dz = fz - 470.0f;
        float mountainDist = sqrt(dx * dx + dz * dz);

        if (mountainDist < 150.0f) {
            float mountain =
                (1.0f - mountainDist / 150.0f) * 130.0f;

            mountain *= mountain;

            h += mountain;
        }

        int height = (int)h;

        // Stone body
        cube(
            glm::ivec3(x, 30, z),
            glm::ivec3(x + 16, height - 6, z + 16),
            stone
        );

        // Dirt layer
        cube(
            glm::ivec3(x, height - 6, z),
            glm::ivec3(x + 16, height - 2, z + 16),
            dirt
        );

        // Grass
        cube(
            glm::ivec3(x, height - 2, z),
            glm::ivec3(x + 16, height + 1, z + 16),
            grass
        );
    }
}

// ============================================================
// LAKE
// ============================================================

cube(
    glm::ivec3(60, 45, 380),
    glm::ivec3(270, 49, 540),
    water
);

// Lake shoreline
cube(
    glm::ivec3(48, 43, 368),
    glm::ivec3(282, 46, 552),
    sand
);

// Put water back on top
cube(
    glm::ivec3(60, 47, 380),
    glm::ivec3(270, 51, 540),
    water
);

// ============================================================
// CENTRAL VILLAGE
// ============================================================

const int villageX = 210;
const int villageZ = 180;
const int villageY = 90;

// Main road
cube(
    glm::ivec3(villageX - 100, villageY, villageZ + 30),
    glm::ivec3(villageX + 110, villageY + 3, villageZ + 50),
    road
);

cube(
    glm::ivec3(villageX + 20, villageY, villageZ - 80),
    glm::ivec3(villageX + 40, villageY + 3, villageZ + 110),
    road
);

// Cross road
cube(
    glm::ivec3(villageX - 40, villageY, villageZ - 20),
    glm::ivec3(villageX + 100, villageY + 3, villageZ),
    road
);

// Houses
house(villageX - 80, villageY + 3, villageZ - 60, 45, 40);
house(villageX + 55, villageY + 3, villageZ - 55, 45, 40);
house(villageX - 80, villageY + 3, villageZ + 65, 45, 40);
house(villageX + 55, villageY + 3, villageZ + 60, 45, 40);

// Small central plaza
cube(
    glm::ivec3(villageX - 25, villageY + 3, villageZ - 25),
    glm::ivec3(villageX + 70, villageY + 6, villageZ + 70),
    stone
);

// Fountain
cube(
    glm::ivec3(villageX + 10, villageY + 6, villageZ + 10),
    glm::ivec3(villageX + 35, villageY + 10, villageZ + 35),
    stoneDark
);

cube(
    glm::ivec3(villageX + 14, villageY + 10, villageZ + 14),
    glm::ivec3(villageX + 31, villageY + 12, villageZ + 31),
    water
);

// ============================================================
// CENTRAL CASTLE
// ============================================================

const int castleX = 430;
const int castleZ = 150;
const int castleY = 130;

// Castle floor
cube(
    glm::ivec3(castleX - 65, castleY, castleZ - 65),
    glm::ivec3(castleX + 65, castleY + 8, castleZ + 65),
    stoneDark
);

// Castle walls
cube(
    glm::ivec3(castleX - 60, castleY + 8, castleZ - 60),
    glm::ivec3(castleX + 60, castleY + 45, castleZ - 45),
    stone
);

cube(
    glm::ivec3(castleX - 60, castleY + 8, castleZ + 45),
    glm::ivec3(castleX + 60, castleY + 45, castleZ + 60),
    stone
);

cube(
    glm::ivec3(castleX - 60, castleY + 8, castleZ - 60),
    glm::ivec3(castleX - 45, castleY + 45, castleZ + 60),
    stone
);

cube(
    glm::ivec3(castleX + 45, castleY + 8, castleZ - 60),
    glm::ivec3(castleX + 60, castleY + 45, castleZ + 60),
    stone
);

// Four towers
tower(castleX - 65, castleY, castleZ - 65);
tower(castleX + 40, castleY, castleZ - 65);
tower(castleX - 65, castleY, castleZ + 40);
tower(castleX + 40, castleY, castleZ + 40);

// Castle entrance
cube(
    glm::ivec3(castleX - 12, castleY + 8, castleZ - 70),
    glm::ivec3(castleX + 12, castleY + 35, castleZ - 43),
    woodDark
);

// ============================================================
// TREES
// ============================================================

// Forest on the western side
for (int x = 30; x < 180; x += 28) {
    for (int z = 40; z < 300; z += 31) {

        // Avoid village
        if (x > 100 && x < 350 &&
            z > 100 && z < 300)
            continue;

        float fx = (float)x;
        float fz = (float)z;

        int y =
            65
            + (int)(
                sin(fx * 0.018f) * 22.0f
                + sin(fz * 0.021f) * 18.0f
                + sin((fx + fz) * 0.010f) * 25.0f
            );

        tree(x, y, z, 10 + ((x + z) % 6));
    }
}

// Forest around lake
for (int x = 300; x < 600; x += 35) {
    for (int z = 330; z < 600; z += 37) {

        // Keep lake open
        if (x > 40 && x < 290 &&
            z > 360 && z < 550)
            continue;

        float fx = (float)x;
        float fz = (float)z;

        int y =
            65
            + (int)(
                sin(fx * 0.018f) * 22.0f
                + sin(fz * 0.021f) * 18.0f
                + sin((fx + fz) * 0.010f) * 25.0f
            );

        tree(x, y, z, 11 + ((x * 3 + z) % 7));
    }
}

// ============================================================
// MOUNTAIN PEAKS
// ============================================================

// Large snowy-looking stone peaks
for (int i = 0; i < 5; i++) {

    int x = 420 + i * 30;
    int z = 430 + (i % 2) * 35;

    int baseY = 150 + i * 8;

    for (int r = 50; r > 5; r -= 10) {

        int y = baseY + (50 - r) * 2;

        cube(
            glm::ivec3(x - r, y, z - r),
            glm::ivec3(x + r, y + 12, z + r),
            stone
        );
    }

    // Peak
    cube(
        glm::ivec3(x - 10, baseY + 90, z - 10),
        glm::ivec3(x + 10, baseY + 125, z + 10),
        stone
    );
}

// ============================================================
// FLOATING ISLAND
// ============================================================

cube(
    glm::ivec3(70, 250, 70),
    glm::ivec3(150, 270, 150),
    stone
);

cube(
    glm::ivec3(82, 270, 82),
    glm::ivec3(138, 278, 138),
    dirt
);

cube(
    glm::ivec3(82, 278, 82),
    glm::ivec3(138, 282, 138),
    grass
);

// Tree on floating island
tree(110, 282, 110, 18);

// Hanging underside
for (int r = 35; r > 0; r -= 7) {

    int y = 250 - (35 - r);

    cube(
        glm::ivec3(110 - r, y, 110 - r),
        glm::ivec3(110 + r, y + 8, 110 + r),
        stoneDark
    );
}

// ============================================================
// SMALL RUINS
// ============================================================

for (int i = 0; i < 7; i++) {

    int x = 300 + i * 23;
    int z = 70 + (i % 3) * 30;

    cube(
        glm::ivec3(x, 100, z),
        glm::ivec3(x + 10, 130 + (i % 3) * 10, z + 10),
        stoneDark
    );

    if (i % 2 == 0) {
        cube(
            glm::ivec3(x + 14, 100, z),
            glm::ivec3(x + 24, 120, z + 10),
            stone
        );
    }
}

// ============================================================
// TORCHES / LIGHTS AROUND VILLAGE
// ============================================================

for (int x = villageX - 90; x <= villageX + 100; x += 30) {

    cube(
        glm::ivec3(x, villageY + 4, villageZ + 25),
        glm::ivec3(x + 3, villageY + 15, villageZ + 28),
        woodDark
    );

    cube(
        glm::ivec3(x - 2, villageY + 15, villageZ + 23),
        glm::ivec3(x + 5, villageY + 20, villageZ + 30),
        torch
    );
}

// ============================================================
// BRIDGE OVER LAKE
// ============================================================

cube(
    glm::ivec3(260, 55, 430),
    glm::ivec3(350, 61, 450),
    wood
);

for (int x = 260; x <= 350; x += 15) {

    cube(
        glm::ivec3(x, 50, 430),
        glm::ivec3(x + 5, 55, 435),
        woodDark
    );

    cube(
        glm::ivec3(x, 50, 445),
        glm::ivec3(x + 5, 55, 450),
        woodDark
    );
}

// ============================================================
// FINAL GRASS PATCHES
// ============================================================

// Break up the perfectly flat terrain with small elevated
// patches around the world.

for (int x = 20; x < 620; x += 43) {
    for (int z = 20; z < 620; z += 47) {

        // Don't clutter major structures
        if (x > 130 && x < 340 &&
            z > 100 && z < 300)
            continue;

        if (x > 390 && z > 100 && z < 300)
            continue;

        cube(
            glm::ivec3(x, 82, z),
            glm::ivec3(x + 8, 86, z + 8),
            grassDark
        );
    }
}

    posBuffer = renderer.CreateResource<TypedBuffer<glm::vec3>>();
    posBuffer->usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    posBuffer->SetSize(1);
    posBuffer->Create();

    TypedBuffer<ContreeNode> *nodes = renderer.CreateResource<TypedBuffer<ContreeNode>>();
    nodes->usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    nodes->SetSize(vm.contree_data.size());
    nodes->Create();
    nodes->Upload(vm.contree_data);

    TypedBuffer<Chunk> *chunks = renderer.CreateResource<TypedBuffer<Chunk>>();
    chunks->usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    chunks->SetSize(vm.allocated_chunks.size());
    chunks->Create();
    chunks->Upload(vm.allocated_chunks);

    TypedBuffer<ChunkPositionsHeader> *chunkPositionsHeader = renderer.CreateResource<TypedBuffer<ChunkPositionsHeader>>();
    chunkPositionsHeader->usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    chunkPositionsHeader->SetSize(1);
    chunkPositionsHeader->Create();
    chunkPositionsHeader->Upload((ChunkPositionsHeader*)&vm.chunk_occupancy, 1);

    TypedBuffer<uint32_t> *chunkPositions = renderer.CreateResource<TypedBuffer<uint32_t>>();
    chunkPositions->usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    chunkPositions->SetSize(vm.chunk_occupancy.get_size());
    chunkPositions->Create();
    chunkPositions->Upload((uint32_t*)vm.chunk_occupancy.chunks, vm.chunk_occupancy.get_size());

    ComputePass *depthPass = renderer.CreateShaderPass<ComputePass>();
    depthPass->spirv = depth_spirv;
    depthPass->spirv_size = depth_spirv_sizeInBytes/4;
    depthPass->threadcount = {16, 16, 1};
    depthPass->readwrite_storage_textures.push_back(halfDepth);
    depthPass->dispatchFunc = [this](const ComputePass& pass) {
        Window &w = GetModule<Window>();
        glm::ivec2 size = w.GetSize()/2;
        return glm::uvec3(
            ((size.x)+pass.threadcount.x-1)/pass.threadcount.x,
            ((size.y)+pass.threadcount.y-1)/pass.threadcount.y,
            1
        );
    };
    depthPass->readonly_storage_buffers.push_back(nodes);
    depthPass->readonly_storage_buffers.push_back(chunks);
    depthPass->readonly_storage_buffers.push_back(chunkPositionsHeader);
    depthPass->readonly_storage_buffers.push_back(chunkPositions);
    depthPass->readonly_storage_buffers.push_back(posBuffer);
    depthPass->Create();
    
    ComputePass *depthUpscale = renderer.CreateShaderPass<ComputePass>();
    depthUpscale->spirv = upscale_spirv;
    depthUpscale->spirv_size = upscale_spirv_sizeInBytes/4;
    depthUpscale->threadcount = {8, 8, 1};
    depthUpscale->readwrite_storage_textures.push_back(halfDepth);
    depthUpscale->readwrite_storage_textures.push_back(fullDepth);
    depthUpscale->dispatchFunc = [this](const ComputePass& pass) {
        Window &w = GetModule<Window>();
        glm::ivec2 size = w.GetSize();
        return glm::uvec3(
            ((size.x)+pass.threadcount.x-1)/pass.threadcount.x,
            ((size.y)+pass.threadcount.y-1)/pass.threadcount.y,
            1
        );
    };
    depthUpscale->Create();


    ComputePass *primaryPass = renderer.CreateShaderPass<ComputePass>();
    primaryPass->spirv = primary_spirv;
    primaryPass->spirv_size = primary_spirv_sizeInBytes/4;
    primaryPass->threadcount = {8, 8, 1};
    primaryPass->readwrite_storage_textures.push_back(fullDepth);
    primaryPass->readwrite_storage_textures.push_back(display);
    primaryPass->dispatchFunc = [this](const ComputePass& pass) {
        Window &w = GetModule<Window>();
        glm::ivec2 size = w.GetSize();
        return glm::uvec3(
            ((size.x)+pass.threadcount.x-1)/pass.threadcount.x,
            ((size.y)+pass.threadcount.y-1)/pass.threadcount.y,
            1
        );
    };
    primaryPass->readonly_storage_buffers.push_back(nodes);
    primaryPass->readonly_storage_buffers.push_back(chunks);
    primaryPass->readonly_storage_buffers.push_back(chunkPositionsHeader);
    primaryPass->readonly_storage_buffers.push_back(chunkPositions);
    primaryPass->readonly_storage_buffers.push_back(posBuffer);
    primaryPass->Create();



    BlitPass *copyToSwaptex = renderer.CreateShaderPass<BlitPass>();
    copyToSwaptex->source = display;
    copyToSwaptex->destination = &renderer.swapchainTexture;

    ImGuiPass *gui = renderer.CreateShaderPass<ImGuiPass>();
    gui->destination = &renderer.swapchainTexture;

    pos = {0, 0, 0};

    window.ResizedScreen.Bind(
        [this, display, halfDepth, fullDepth](glm::ivec2 size) {
            display->size = size;
            display->Create();

            halfDepth->size = size / 2;
            halfDepth->Create();

            fullDepth->size = size;
            fullDepth->Create();
        }
    );
}

void VoxelRenderer::Process() {
    Input &input = GetModule<Input>();
    float deltaTime = GetModule<DeltaTime>().Get()*10;
    if (input.IsHeld("break_block")) {
        deltaTime *= 5;
    }
    if (input.IsHeld("left")) {
        pos.x -= deltaTime;
    }
    if (input.IsHeld("right")) {
        pos.x += deltaTime;
    }
    if (input.IsHeld("forward")) {
        pos.z += deltaTime;
    }
    if (input.IsHeld("backward")) {
        pos.z -= deltaTime;
    }
    if (input.IsHeld("up")) {
        pos.y += deltaTime;
    }
    if (input.IsHeld("down")) {
        pos.y -= deltaTime;
    }

    posBuffer->Upload(&pos, 1);

    static float elapsed = 0.0f;
    static uint32_t frames = 0;

    float dt = GetModule<DeltaTime>().Get();

    elapsed += dt;
    frames++;

    if (elapsed >= 0.1f)
    {
        float fps = frames / elapsed;

        Console &console = GetModule<Console>();
        console.Log(std::to_string(fps), Console::LogLevel::Info);

        elapsed = 0.0f;
        frames = 0;
    }
}

void VoxelRenderer::Shutdown() {
    
}
