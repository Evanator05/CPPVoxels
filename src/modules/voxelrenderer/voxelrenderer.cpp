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
#include "FastNoiseLite.h"
#include <cstdlib>
#include <ctime>
#include <cmath>
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
    GenerateCaves(vm);

    posBuffer = renderer.CreateResource<TypedBuffer<CameraTransform>>();
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

    cameraTransform.localPos = {0, 0, 0};

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

void VoxelRenderer::Process()
{
    Input& input = GetModule<Input>();
    input.SetMouseLock(true);
    glm::vec2 mouseMovement = input.GetMouseMovement();

    float deltaTime = GetModule<DeltaTime>().Get() * 50.0f;

    if (input.IsHeld("break_block"))
        deltaTime *= 5.0f;

    // ------------------------------------------------------------
    // Camera rotation
    // ------------------------------------------------------------

    static float yaw = 0.0f;
static float pitch = 0.0f;

constexpr float mouseSensitivity = 0.0025f;

yaw   += mouseMovement.x * mouseSensitivity;
pitch -= mouseMovement.y * mouseSensitivity;

constexpr float pitchLimit = glm::half_pi<float>() - 0.001f;
pitch = glm::clamp(pitch, -pitchLimit, pitchLimit);

// Basis vectors
glm::vec3 forward;
forward.x = cos(pitch) * sin(yaw);
forward.y = sin(pitch);
forward.z = cos(pitch) * cos(yaw);

glm::vec3 worldUp(0.0f, 1.0f, 0.0f);

glm::vec3 right = glm::normalize(glm::cross(worldUp, forward));
glm::vec3 up    = glm::normalize(glm::cross(forward, right));

// Store as COLUMNS
cameraTransform.rotation0 = right;
cameraTransform.rotation1 = up;
cameraTransform.rotation2 = forward;
    // ------------------------------------------------------------
    // Movement
    // ------------------------------------------------------------

   glm::vec3 movement(0.0f);

if (input.IsHeld("left"))
    movement.x -= 1.0f;

if (input.IsHeld("right"))
    movement.x += 1.0f;

if (input.IsHeld("forward"))
    movement.z += 1.0f;

if (input.IsHeld("backward"))
    movement.z -= 1.0f;

if (input.IsHeld("up"))
    movement.y += 1.0f;

if (input.IsHeld("down"))
    movement.y -= 1.0f;

glm::vec3 worldMovement =
    right * movement.x +
    up * movement.y +
    forward * movement.z;

cameraTransform.localPos += worldMovement * deltaTime;

    // ------------------------------------------------------------
    // Upload
    // ------------------------------------------------------------

    posBuffer->Upload(&cameraTransform, 1);

    // ------------------------------------------------------------
    // FPS
    // ------------------------------------------------------------

    static float elapsed = 0.0f;
    static uint32_t frames = 0;

    float dt = GetModule<DeltaTime>().Get();

    elapsed += dt;
    frames++;

    if (elapsed >= 0.1f)
    {
        float fps = frames / elapsed;

        Console& console = GetModule<Console>();
        console.Log(
            "FPS: " + std::to_string((int)std::round(fps)),
            Console::LogLevel::Info
        );

        elapsed = 0.0f;
        frames = 0;
    }
}

void VoxelRenderer::Shutdown() {
    
}
