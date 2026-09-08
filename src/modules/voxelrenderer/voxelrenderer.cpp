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
#include "shaders/probelighting.h"
#include "shaders/probecombine.h"
#include "shaders/denoise.h"
#include "testgeneration.h"

void VoxelRenderer::Init() {
    Window &window = GetModule<Window>();
    Renderer &renderer = GetModule<Renderer>();

    device = renderer.GetDevice();
    
    Texture *display = renderer.CreateResource<Texture>();
    display->size = window.GetSize();
    display->usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    display->format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    display->Create();

    Texture *albedo = renderer.CreateResource<Texture>();
    albedo->size = window.GetSize();
    albedo->usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    albedo->format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    albedo->Create();

    Texture *probeIndex = renderer.CreateResource<Texture>();
    probeIndex->size = window.GetSize();
    probeIndex->usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    probeIndex->format = SDL_GPU_TEXTUREFORMAT_R32_UINT;
    probeIndex->Create();

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
    Generator::LoadVoxFile(vm, "castle.vox");
    //Generator::GenerateCaves(vm);

    posBuffer = renderer.CreateResource<TypedBuffer<CameraTransform>>();
    posBuffer->usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    posBuffer->SetSize(1);
    posBuffer->Create();

    TypedBuffer<ContreeHeader> *contree_headers = renderer.CreateResource<TypedBuffer<ContreeHeader>>();
    contree_headers->usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    contree_headers->SetSize(vm.contree_headers.size());
    contree_headers->Create();
    contree_headers->Upload(vm.contree_headers);

    TypedBuffer<ContreeData> *contree_data = renderer.CreateResource<TypedBuffer<ContreeData>>();
    contree_data->usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    contree_data->SetSize(vm.contree_data.size());
    contree_data->Create();
    contree_data->Upload(vm.contree_data);

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

    TypedBuffer<FaceEntry> *faceEntries = renderer.CreateResource<TypedBuffer<FaceEntry>>();
    faceEntries->usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
    faceEntries->SetSize(window.GetSize().x*window.GetSize().y*4);
    faceEntries->Create();

    TypedBuffer<FaceEntry> *smoothFaces = renderer.CreateResource<TypedBuffer<FaceEntry>>();
    smoothFaces->usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
    smoothFaces->SetSize(window.GetSize().x*window.GetSize().y*4);
    smoothFaces->Create();

    ComputePass *depthPass = renderer.CreateShaderPass<ComputePass>();
    depthPass->spirv = depth_spirv;
    depthPass->spirv_size = depth_spirv_sizeInBytes/4;
    depthPass->threadcount = {8, 8, 1};
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
    depthPass->readonly_storage_buffers.push_back(posBuffer);
    depthPass->readonly_storage_buffers.push_back(contree_headers);
    depthPass->readonly_storage_buffers.push_back(contree_data);
    depthPass->readonly_storage_buffers.push_back(chunks);
    depthPass->readonly_storage_buffers.push_back(chunkPositionsHeader);
    depthPass->readonly_storage_buffers.push_back(chunkPositions);
    
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
    primaryPass->readwrite_storage_textures.push_back(albedo);
    primaryPass->readwrite_storage_textures.push_back(probeIndex);
    primaryPass->dispatchFunc = [this](const ComputePass& pass) {
        Window &w = GetModule<Window>();
        glm::ivec2 size = w.GetSize();
        return glm::uvec3(
            ((size.x)+pass.threadcount.x-1)/pass.threadcount.x,
            ((size.y)+pass.threadcount.y-1)/pass.threadcount.y,
            1
        );
    };
    primaryPass->readonly_storage_buffers.push_back(posBuffer);
    primaryPass->readonly_storage_buffers.push_back(contree_headers);
    primaryPass->readonly_storage_buffers.push_back(contree_data);
    primaryPass->readonly_storage_buffers.push_back(chunks);
    primaryPass->readonly_storage_buffers.push_back(chunkPositionsHeader);
    primaryPass->readonly_storage_buffers.push_back(chunkPositions);
    primaryPass->readwrite_storage_buffers.push_back(faceEntries);
    primaryPass->Create();

    ComputePass* probeLightingPass = renderer.CreateShaderPass<ComputePass>();
    probeLightingPass->spirv = probelighting_spirv;
    probeLightingPass->spirv_size = probelighting_spirv_sizeInBytes / 4;
    probeLightingPass->threadcount = {64, 1, 1};
    probeLightingPass->readonly_storage_buffers.push_back(posBuffer);
    probeLightingPass->readonly_storage_buffers.push_back(contree_headers);
    probeLightingPass->readonly_storage_buffers.push_back(contree_data);
    probeLightingPass->readonly_storage_buffers.push_back(chunks);
    probeLightingPass->readonly_storage_buffers.push_back(chunkPositionsHeader);
    probeLightingPass->readonly_storage_buffers.push_back(chunkPositions);
    probeLightingPass->readwrite_storage_buffers.push_back(faceEntries);
    probeLightingPass->dispatchFunc =
        [faceEntries](const ComputePass&) {
            const size_t threads = 64;
            const size_t groupsPerRow = 65535;
            const size_t groupCount = (faceEntries->GetSize() + threads - 1) / threads;
            const size_t groupsX = groupCount < groupsPerRow ? groupCount : groupsPerRow;
            const size_t groupsY = (groupCount + groupsPerRow - 1) / groupsPerRow;
            return glm::uvec3(static_cast<uint32_t>(groupsX), static_cast<uint32_t>(groupsY), 1);
        };
    probeLightingPass->Create();

    ComputePass* denoisePass = renderer.CreateShaderPass<ComputePass>();
    denoisePass->spirv = denoise_spirv;
    denoisePass->spirv_size = denoise_spirv_sizeInBytes / 4;
    denoisePass->threadcount = {64, 1, 1};
    denoisePass->readonly_storage_buffers.push_back(posBuffer);
    denoisePass->readonly_storage_buffers.push_back(contree_headers);
    denoisePass->readonly_storage_buffers.push_back(contree_data);
    denoisePass->readonly_storage_buffers.push_back(chunks);
    denoisePass->readonly_storage_buffers.push_back(chunkPositionsHeader);
    denoisePass->readonly_storage_buffers.push_back(chunkPositions);
    denoisePass->readwrite_storage_buffers.push_back(faceEntries);
    denoisePass->readwrite_storage_buffers.push_back(smoothFaces);
    denoisePass->dispatchFunc =
        [faceEntries](const ComputePass&) {
            const size_t threads = 64;
            const size_t groupsPerRow = 65535;
            const size_t groupCount = (faceEntries->GetSize() + threads - 1) / threads;
            const size_t groupsX = groupCount < groupsPerRow ? groupCount : groupsPerRow;
            const size_t groupsY = (groupCount + groupsPerRow - 1) / groupsPerRow;
            return glm::uvec3(static_cast<uint32_t>(groupsX), static_cast<uint32_t>(groupsY), 1);
        };
    denoisePass->Create();

    ComputePass* combinePass = renderer.CreateShaderPass<ComputePass>();
    combinePass->spirv = probecombine_spirv;
    combinePass->spirv_size = probecombine_spirv_sizeInBytes / 4;
    combinePass->threadcount = {8, 8, 1};
    combinePass->readonly_storage_textures.push_back(albedo);
    combinePass->readonly_storage_textures.push_back(probeIndex);
    combinePass->readonly_storage_buffers.push_back(smoothFaces);
    combinePass->readwrite_storage_textures.push_back(display);
    combinePass->dispatchFunc = [display](const ComputePass&) {
        return glm::uvec3(
            (display->size.x + 7) / 8,
            (display->size.y + 7) / 8,
            1
        );
    };
    combinePass->Create();

    BlitPass *copyToSwaptex = renderer.CreateShaderPass<BlitPass>();
    copyToSwaptex->source = display;
    copyToSwaptex->destination = &renderer.swapchainTexture;

    ImGuiPass *gui = renderer.CreateShaderPass<ImGuiPass>();
    gui->destination = &renderer.swapchainTexture;

    cameraTransform.localPos = {0, 150, 0};

    window.ResizedScreen.Bind(
        [this, albedo, display, halfDepth, fullDepth, faceEntries, probeIndex, smoothFaces](glm::ivec2 size) {
            albedo->size = size;
            albedo->Create();

            probeIndex->size = size;
            probeIndex->Create();

            display->size = size;
            display->Create();

            halfDepth->size = size / 2;
            halfDepth->Create();

            fullDepth->size = size;
            fullDepth->Create();

            faceEntries->SetSize(size.x*size.y*4);
            faceEntries->Create();
            smoothFaces->SetSize(size.x*size.y*4);
            smoothFaces->Create();
        }
    );
}

void VoxelRenderer::Process() {
    Input& input = GetModule<Input>();
    input.SetMouseLock(true);
    glm::vec2 mouseMovement = input.GetMouseMovement();

    float deltaTime = GetModule<DeltaTime>().Get();
    float moveSpeed = 50.0f;
    cameraTransform.time += deltaTime;
    if (input.IsHeld("break_block"))
        moveSpeed *= 5.0f;

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

    cameraTransform.localPos += worldMovement * moveSpeed * deltaTime;
    cameraTransform.frame++;
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
