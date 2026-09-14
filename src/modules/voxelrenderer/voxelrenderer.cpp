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
#include "shaders/clearactiveprobes.h"
#include "shaders/primary.h"
#include "shaders/setindirecttraceprobelighting.h"
#include "shaders/traceprobelighting.h"
#include "shaders/combinelighting.h"
#include "testgeneration.h"

#include <bit>

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

    Texture *positions = renderer.CreateResource<Texture>();
    positions->size = window.GetSize();
    positions->usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    positions->format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_INT;
    positions->Create();

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
    Voxel v;
    v.set_rgb(0, 0, 31);
    v.set_solid(true);
    v.set_type(Voxel::Type::Emissive);
    v.set_payload(31);
    vm.FillVoxels(glm::ivec3(-24, 157, 108), glm::ivec3(-24-10, 157-5, 108-10), v);
    //Generator::GenerateCaves(vm);
    CreateLightChunks();

    cameraTransformBuffer = renderer.CreateResource<TypedBuffer<CameraTransform>>();
    cameraTransformBuffer->usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    cameraTransformBuffer->SetSize(1);
    cameraTransformBuffer->Create();

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

    TypedBuffer<uint32_t> *activeProbesBuffer = renderer.CreateResource<TypedBuffer<uint32_t>>();
    activeProbesBuffer->usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
    activeProbesBuffer->SetSize(window.GetSize().x * window.GetSize().y);
    activeProbesBuffer->Create();

    TypedBuffer<uint32_t> *indirectArgsBuffer = renderer.CreateResource<TypedBuffer<uint32_t>>();
    indirectArgsBuffer->usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_BUFFERUSAGE_INDIRECT;
    indirectArgsBuffer->SetSize(3);
    indirectArgsBuffer->Create();

    window.ResizedScreen.Bind(
        [this, albedo, display, halfDepth, fullDepth, positions, activeProbesBuffer](glm::ivec2 size) {
            albedo->size = size;
            albedo->Create();

            positions->size = size;
            positions->Create();

            display->size = size;
            display->Create();

            halfDepth->size = size / 2;
            halfDepth->Create();

            fullDepth->size = size;
            fullDepth->Create();

            activeProbesBuffer->SetSize(size.x * size.y);
            activeProbesBuffer->Create();
        }
    );

    ComputePass *depthPass = renderer.CreateShaderPass<ComputePass>();
    depthPass->spirv = depth_spirv;
    depthPass->spirv_size = depth_spirv_sizeInBytes/4;
    depthPass->threadcount = {8, 8, 1};
    depthPass->dispatchFunc = [this](const ComputePass& pass) {
        Window &w = GetModule<Window>();
        glm::ivec2 size = w.GetSize()/2;
        return glm::uvec3(
            ((size.x)+pass.threadcount.x-1)/pass.threadcount.x,
            ((size.y)+pass.threadcount.y-1)/pass.threadcount.y,
            1
        );
    };
    depthPass->readonly_storage_buffers.push_back(cameraTransformBuffer);
    depthPass->readonly_storage_buffers.push_back(contree_headers);
    depthPass->readonly_storage_buffers.push_back(contree_data);
    depthPass->readonly_storage_buffers.push_back(chunks);
    depthPass->readonly_storage_buffers.push_back(chunkPositionsHeader);
    depthPass->readonly_storage_buffers.push_back(chunkPositions);
    depthPass->readwrite_storage_textures.push_back(halfDepth);
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

    ComputePass *clearActiveProbesPass = renderer.CreateShaderPass<ComputePass>();
    clearActiveProbesPass->spirv = clearactiveprobes_spirv;
    clearActiveProbesPass->spirv_size = clearactiveprobes_spirv_sizeInBytes/4;
    clearActiveProbesPass->threadcount = {1, 1, 1};
    clearActiveProbesPass->dispatchFunc = [](const ComputePass& pass) {return glm::uvec3(1); };
    clearActiveProbesPass->readwrite_storage_buffers.push_back(activeProbesBuffer);
    clearActiveProbesPass->Create();

    ComputePass *primaryPass = renderer.CreateShaderPass<ComputePass>();
    primaryPass->spirv = primary_spirv;
    primaryPass->spirv_size = primary_spirv_sizeInBytes/4;
    primaryPass->threadcount = {8, 8, 1};
    primaryPass->dispatchFunc = [this](const ComputePass& pass) {
        Window &w = GetModule<Window>();
        glm::ivec2 size = w.GetSize();
        return glm::uvec3(
            ((size.x)+pass.threadcount.x-1)/pass.threadcount.x,
            ((size.y)+pass.threadcount.y-1)/pass.threadcount.y,
            1
        );
    };
    primaryPass->readonly_storage_buffers.push_back(cameraTransformBuffer);
    primaryPass->readonly_storage_buffers.push_back(contree_headers);
    primaryPass->readonly_storage_buffers.push_back(contree_data);
    primaryPass->readonly_storage_buffers.push_back(chunks);
    primaryPass->readonly_storage_buffers.push_back(chunkPositionsHeader);
    primaryPass->readonly_storage_buffers.push_back(chunkPositions);
    primaryPass->readwrite_storage_textures.push_back(fullDepth);
    primaryPass->readwrite_storage_textures.push_back(albedo);
    primaryPass->readwrite_storage_textures.push_back(positions);
    primaryPass->readwrite_storage_buffers.push_back(lightChunksBuffer);
    primaryPass->readwrite_storage_buffers.push_back(activeProbesBuffer);
    primaryPass->Create();

    ComputePass *setIndirectTraceProbeLighting = renderer.CreateShaderPass<ComputePass>();
    setIndirectTraceProbeLighting->spirv = setindirecttraceprobelighting_spirv;
    setIndirectTraceProbeLighting->spirv_size = setindirecttraceprobelighting_spirv_sizeInBytes/4;
    setIndirectTraceProbeLighting->threadcount = {1, 1, 1};
    setIndirectTraceProbeLighting->dispatchFunc = [](const ComputePass& pass) {return glm::uvec3(1); };
    setIndirectTraceProbeLighting->readwrite_storage_buffers.push_back(activeProbesBuffer);
    setIndirectTraceProbeLighting->readwrite_storage_buffers.push_back(indirectArgsBuffer);
    setIndirectTraceProbeLighting->Create();

    ComputePass *traceProbeLightingPass = renderer.CreateShaderPass<ComputePass>();
    traceProbeLightingPass->spirv = traceprobelighting_spirv;
    traceProbeLightingPass->spirv_size = traceprobelighting_spirv_sizeInBytes/4;
    traceProbeLightingPass->threadcount = {64, 1, 1};
    traceProbeLightingPass->indirect_dispatch_buffer = indirectArgsBuffer;
    traceProbeLightingPass->readonly_storage_buffers.push_back(cameraTransformBuffer);
    traceProbeLightingPass->readonly_storage_buffers.push_back(contree_headers);
    traceProbeLightingPass->readonly_storage_buffers.push_back(contree_data);
    traceProbeLightingPass->readonly_storage_buffers.push_back(chunks);
    traceProbeLightingPass->readonly_storage_buffers.push_back(chunkPositionsHeader);
    traceProbeLightingPass->readonly_storage_buffers.push_back(chunkPositions);
    traceProbeLightingPass->readwrite_storage_buffers.push_back(lightChunksBuffer);
    traceProbeLightingPass->readwrite_storage_buffers.push_back(activeProbesBuffer);
    traceProbeLightingPass->Create();

    ComputePass *combineLightingPass = renderer.CreateShaderPass<ComputePass>();
    combineLightingPass->spirv = combinelighting_spirv;
    combineLightingPass->spirv_size = combinelighting_spirv_sizeInBytes/4;
    combineLightingPass->threadcount = {8, 8, 1};
    combineLightingPass->dispatchFunc = [this](const ComputePass& pass) {
        Window &w = GetModule<Window>();
        glm::ivec2 size = w.GetSize();
        return glm::uvec3(
            ((size.x)+pass.threadcount.x-1)/pass.threadcount.x,
            ((size.y)+pass.threadcount.y-1)/pass.threadcount.y,
            1
        );
    };
    combineLightingPass->readonly_storage_textures.push_back(albedo);
    combineLightingPass->readonly_storage_textures.push_back(positions);
    combineLightingPass->readonly_storage_buffers.push_back(chunkPositionsHeader);
    combineLightingPass->readonly_storage_buffers.push_back(chunkPositions);
    combineLightingPass->readwrite_storage_textures.push_back(display);
    combineLightingPass->readwrite_storage_buffers.push_back(lightChunksBuffer);
    combineLightingPass->Create();


    BlitPass *copyToSwaptex = renderer.CreateShaderPass<BlitPass>();
    copyToSwaptex->source = display;
    copyToSwaptex->destination = &renderer.swapchainTexture;

    ImGuiPass *gui = renderer.CreateShaderPass<ImGuiPass>();
    gui->destination = &renderer.swapchainTexture;

    cameraTransform.localPos = {0, 150, 0};
}

void VoxelRenderer::Process() {
    Input& input = GetModule<Input>();
    input.SetMouseLock(true);

    const float deltaTime = GetModule<DeltaTime>().Get();

    float moveSpeed = 50.0f;
    cameraTransform.time += deltaTime;

    if (input.IsHeld("speedmodifier"))
        moveSpeed *= 5.0f;

    // ------------------------------------------------------------
    // Camera rotation
    // ------------------------------------------------------------

    static float yaw = 0.0f;
    static float pitch = 0.0f;

    // Scales both mouse and controller look.
    constexpr float lookSpeed = 1.25f;

    const float lookX =
        input.GetDelta("lookright", deltaTime) -
        input.GetDelta("lookleft", deltaTime);

    const float lookY =
        input.GetDelta("lookup", deltaTime) -
        input.GetDelta("lookdown", deltaTime);

    yaw   += lookX * lookSpeed;
    pitch += lookY * lookSpeed;

    constexpr float pitchLimit = glm::half_pi<float>() - 0.001f;
    pitch = glm::clamp(pitch, -pitchLimit, pitchLimit);

    glm::vec3 forward;
    forward.x = cos(pitch) * sin(yaw);
    forward.y = sin(pitch);
    forward.z = cos(pitch) * cos(yaw);

    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);

    const glm::vec3 right =
        glm::normalize(glm::cross(worldUp, forward));

    const glm::vec3 up =
        glm::normalize(glm::cross(forward, right));

    // Store as columns.
    cameraTransform.rotation0 = right;
    cameraTransform.rotation1 = up;
    cameraTransform.rotation2 = forward;

    // ------------------------------------------------------------
    // Movement
    // ------------------------------------------------------------

    glm::vec3 movement(
        input.GetStrength("right") - input.GetStrength("left"),
        input.GetStrength("up") - input.GetStrength("down"),
        input.GetStrength("forward") - input.GetStrength("backward")
    );

    const float movementLength = glm::length(movement);

    if (movementLength > 1.0f)
        movement /= movementLength;

    const glm::vec3 worldMovement =
        right * movement.x +
        up * movement.y +
        forward * movement.z;

    cameraTransform.localPos += worldMovement * moveSpeed * deltaTime;
    cameraTransform.frame++;

    // ------------------------------------------------------------
    // Upload
    // ------------------------------------------------------------

    cameraTransformBuffer->Upload(&cameraTransform, 1);

    // ------------------------------------------------------------
    // FPS
    // ------------------------------------------------------------

    static float elapsed = 0.0f;
    static uint32_t frames = 0;

    elapsed += deltaTime;
    frames++;

    if (elapsed >= 0.1f) {
        const float fps = frames / elapsed;

        Console& console = GetModule<Console>();
        // console.Log(
        //     "FPS: " + std::to_string((int)std::round(fps)),
        //     Console::LogLevel::Info
        // );

        console.Log("POS " + std::to_string(cameraTransform.localPos.x) + " " + std::to_string(cameraTransform.localPos.y) + " " + std::to_string(cameraTransform.localPos.z), Console::LogLevel::Info);

        elapsed = 0.0f;
        frames = 0;
    }
}

void VoxelRenderer::Shutdown() {
    
}

void VoxelRenderer::CreateLightChunks() {
    Renderer &renderer = GetModule<Renderer>();
    VoxelManager &vm = GetModule<VoxelManager>();
    if (lightChunksBuffer == nullptr) {
        lightChunksBuffer = renderer.CreateResource<TypedBuffer<LightChunk>>();
        lightChunksBuffer->usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
    }

    if (lightChunks.size() != vm.allocated_chunks.size()) {
        lightChunks.resize(vm.allocated_chunks.size());
    }

    for (size_t i = 0; i < lightChunks.size(); ++i) {
        UpdateLightChunk(i);
    }

    if (lightChunksBuffer->GetSize() != lightChunks.size()) {
        lightChunksBuffer->SetSize(lightChunks.size());
        lightChunksBuffer->Create();
    }
    
    lightChunksBuffer->Upload(lightChunks);
}

void VoxelRenderer::UpdateLightChunk(uint32_t chunk_index) {
    VoxelManager &vm = GetModule<VoxelManager>();
    lightChunks[chunk_index].position = vm.allocated_chunks[chunk_index].position;
    for (size_t i = 0; i < Chunk::WIDTH * Chunk::WIDTH * Chunk::WIDTH / (4 * 4 * 4); ++i) {
        UpdateLightProbeSolid(chunk_index, i);
    }

    for (size_t i = 0; i < Chunk::WIDTH * Chunk::WIDTH * Chunk::WIDTH / (4 * 4 * 4); ++i) {
        UpdateLightProbeNormal(chunk_index, i);
    }
}

void VoxelRenderer::UpdateLightProbeSolid(uint32_t chunk_index, uint16_t probe_index) {
    VoxelManager &vm = GetModule<VoxelManager>();

    LightProbe &probe = lightChunks[chunk_index].probes[probe_index];

    uint32_t node_index = vm.allocated_chunks[chunk_index].contree_node;

    glm::uvec3 node_width = glm::uvec3(Chunk::WIDTH);

    glm::uvec3 probe_position(
        probe_index & 0xF,
        (probe_index >> 4) & 0xF,
        (probe_index >> 8) & 0xF
    );
    glm::uvec3 position = probe_position * 4u;

    for (uint8_t depth = 0; depth < ContreeNode::MAX_DEPTH - 1; depth++) {
        node_width /= ContreeNode::WIDTH;

        glm::uvec3 node_position = (position / node_width);
        position -= node_position * node_width;

        uint8_t child_node_index = ContreeNode::GetIndex(node_position);
        ContreeNode node{&vm.contree_headers[node_index], &vm.contree_data[node_index]};

        if (vm.contree_headers[node_index].IsVoxel(child_node_index)) {
            Voxel child_node_voxel = node.GetVoxel(child_node_index);
            probe.solid = child_node_voxel.solid() ? UINT64_MAX : 0ull;
            return;
        }

        node_index = vm.contree_data[node_index].GetPtr(child_node_index);
    }
    probe.solid = vm.contree_headers[node_index].isSolidMask;
    
}

void VoxelRenderer::UpdateLightProbeNormal(
    uint32_t chunk_index,
    uint16_t probe_index
) {
    VoxelManager& vm = GetModule<VoxelManager>();

    const glm::ivec3 directions[6] = {
        { 1,  0,  0}, {-1,  0,  0},
        { 0,  1,  0}, { 0, -1,  0},
        { 0,  0,  1}, { 0,  0, -1}
    };

    glm::ivec3 probePosition(
        probe_index & 15,
        (probe_index >> 4) & 15,
        (probe_index >> 8) & 15
    );

    uint64_t neighbors[6] = {};

    for (int i = 0; i < 6; ++i) {
        glm::ivec3 position = probePosition + directions[i];
        glm::ivec3 chunkPosition =
            vm.allocated_chunks[chunk_index].position;

        uint32_t neighborChunk = chunk_index;
        bool crossedChunk = false;

        for (int axis = 0; axis < 3; ++axis) {
            if (position[axis] < 0) {
                position[axis] += 16;
                --chunkPosition[axis];
                crossedChunk = true;
            } else if (position[axis] >= 16) {
                position[axis] -= 16;
                ++chunkPosition[axis];
                crossedChunk = true;
            }
        }

        if (crossedChunk) {
            glm::ivec3 regionPosition =
                chunkPosition - vm.chunk_occupancy.position;

            if (glm::any(glm::lessThan(regionPosition, glm::ivec3(0))) ||
                glm::any(glm::greaterThanEqual(
                    regionPosition,
                    glm::ivec3(vm.chunk_occupancy.size)
                ))) {
                continue;
            }

            uint32_t width = vm.chunk_occupancy.size.x;
            uint32_t area = width * vm.chunk_occupancy.size.y;

            uint32_t index =
                uint32_t(regionPosition.x) +
                uint32_t(regionPosition.y) * width +
                uint32_t(regionPosition.z) * area;

            neighborChunk = vm.chunk_occupancy.chunks[index];

            if (neighborChunk == UINT32_MAX)
                continue;
        }

        uint32_t neighborProbe =
            uint32_t(position.x) +
            uint32_t(position.y) * 16u +
            uint32_t(position.z) * 256u;

        neighbors[i] =
            lightChunks[neighborChunk].probes[neighborProbe].solid;
    }

    LightProbe& probe = lightChunks[chunk_index].probes[probe_index];
    probe.set_normal(EncodeNormal26(GetNormal(probe.solid, neighbors)));
}

uint8_t VoxelRenderer::EncodeNormal26(glm::ivec3 normal) {
    if (normal != glm::clamp(normal, -1, 1))
        return 31;
    int index = (normal.x + 1)
              + (normal.y + 1) * 3
              + (normal.z + 1) * 9;
    if (index == 13)
        return 31;
    return static_cast<uint8_t>(index > 13 ? index - 1 : index);
}

glm::ivec3 VoxelRenderer::DecodeNormal26(uint8_t normal26) {
    if (normal26 >= 26)
        return glm::ivec3(0);

    int index = normal26 >= 13 ? normal26 + 1 : normal26;

    return glm::ivec3(
        (index % 3) - 1,
        ((index / 3) % 3) - 1,
        (index / 9) - 1
    );
}

#include <bit> // C++20

glm::ivec3 VoxelRenderer::GetNormal(
    uint64_t solid,
    const uint64_t neighbors[6]
) {
    // Each mask covers an entire half, including interior voxels.
    constexpr uint64_t X_LOW  = 0x3333333333333333ULL;
    constexpr uint64_t X_HIGH = 0xCCCCCCCCCCCCCCCCULL;

    constexpr uint64_t Y_LOW  = 0x00FF00FF00FF00FFULL;
    constexpr uint64_t Y_HIGH = 0xFF00FF00FF00FF00ULL;

    constexpr uint64_t Z_LOW  = 0x00000000FFFFFFFFULL;
    constexpr uint64_t Z_HIGH = 0xFFFFFFFF00000000ULL;

    glm::ivec3 normal(
        std::popcount(solid & X_LOW) - std::popcount(solid & X_HIGH),
        std::popcount(solid & Y_LOW) - std::popcount(solid & Y_HIGH),
        std::popcount(solid & Z_LOW) - std::popcount(solid & Z_HIGH)
    );

    // Only fall back when the probe has no net internal direction.
    if (normal == glm::ivec3(0)) {
        // Neighbor order: +X, -X, +Y, -Y, +Z, -Z.
        normal = glm::ivec3(
            std::popcount(neighbors[1]) - std::popcount(neighbors[0]),
            std::popcount(neighbors[3]) - std::popcount(neighbors[2]),
            std::popcount(neighbors[5]) - std::popcount(neighbors[4])
        );
    }

    return glm::sign(normal);
}