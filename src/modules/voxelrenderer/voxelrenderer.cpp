#include "voxelrenderer.h"
#include "modules/renderer/shaderpasses/imguipass.h"
#include "modules/renderer/shaderpasses/blitpass.h"
#include "window.h"
#include "deltatime.h"
#include "input.h"
#include "console.h"
#include "modules/voxel/voxelmanager.h"
#include "filesystem.h"

#include "skyislandgeneration.h"

#include "shaders/depth.h"
#include "shaders/upscale.h"
#include "shaders/clearactiveprobes.h"
#include "shaders/primary.h"
#include "shaders/setindirecttraceprobelighting.h"
#include "shaders/traceprobelighting.h"
#include "shaders/combinelighting.h"
#include "testgeneration.h"

#include <bit>
#include <algorithm>
#include <functional>
#include <limits>
#include <stdexcept>

namespace {
    void RequireGPUOperation(bool success, const char* operation) {
        if (!success)
            throw std::runtime_error(std::string(operation) + ": " + SDL_GetError());
    }

    void GrowTransferBuffer(SDL_GPUDevice* device, SDL_GPUTransferBuffer*& buffer,
                            size_t& capacity, size_t required, SDL_GPUTransferBufferUsage usage) {
        const size_t limit = std::numeric_limits<Uint32>::max();
        if (required > limit)
            throw std::runtime_error("Voxel transfer exceeds SDL's buffer size limit");
        if (buffer && required <= capacity)
            return;

        size_t new_capacity = std::max<size_t>(capacity, 1024 * 1024);
        while (new_capacity < required)
            new_capacity = new_capacity > limit / 2 ? limit : new_capacity * 2;

        SDL_GPUTransferBufferCreateInfo info{};
        info.usage = usage;
        info.size = static_cast<Uint32>(new_capacity);
        auto* replacement = SDL_CreateGPUTransferBuffer(device, &info);
        RequireGPUOperation(replacement != nullptr, "Create voxel transfer buffer");
        if (buffer)
            SDL_ReleaseGPUTransferBuffer(device, buffer);
        buffer = replacement;
        capacity = new_capacity;
    }

    template<class Function>
    void VisitRanges(std::vector<uint32_t>& indices, Function visit) {
        std::sort(indices.begin(), indices.end());
        indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
        for (size_t i = 0; i < indices.size();) {
            const uint32_t first = indices[i++];
            size_t count = 1;
            while (i < indices.size() && indices[i] == first + count) {
                ++i;
                ++count;
            }
            visit(first, count);
        }
    }

    // Renderer::Process runs before VoxelRenderer::Process. Its executable
    // resources provide a synchronization point before any voxel shader reads.
    class VoxelUploadResource : public IExecutableResource {
    public:
        using IExecutableResource::IExecutableResource;
        std::function<void()> synchronize;
        void Create() override {}
        void Destroy() override { synchronize = {}; }
        void Execute(SDL_GPUCommandBuffer*) override { synchronize(); }
    };
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
    
    lightChunksBuffer = renderer.CreateResource<TypedBuffer<LightChunk>>();
    lightChunksBuffer->usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
    lightChunksBuffer->SetSize(1);

    cameraTransformBuffer = renderer.CreateResource<TypedBuffer<CameraTransform>>();
    cameraTransformBuffer->usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    cameraTransformBuffer->SetSize(1);
    cameraTransformBuffer->Create();

    contreeHeaderBuffer = renderer.CreateResource<TypedBuffer<ContreeHeader>>();
    contreeHeaderBuffer->usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    contreeHeaderBuffer->SetSize(std::max<size_t>(1, vm.contree_headers.capacity()));

    contreeDataBuffer = renderer.CreateResource<TypedBuffer<ContreeData>>();
    contreeDataBuffer->usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    contreeDataBuffer->SetSize(std::max<size_t>(1, vm.contree_data.capacity()));

    chunks = renderer.CreateResource<TypedBuffer<Chunk>>();
    chunks->usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    chunks->SetSize(std::max<size_t>(1, vm.allocated_chunks.capacity()));

    chunkPositionsHeader = renderer.CreateResource<TypedBuffer<ChunkPositionsHeader>>();
    chunkPositionsHeader->usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    chunkPositionsHeader->SetSize(1);

    chunkPositions = renderer.CreateResource<TypedBuffer<uint32_t>>();
    chunkPositions->usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    chunkPositions->SetSize(std::max<size_t>(1, vm.chunk_occupancy.get_size()));

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
    depthPass->readonly_storage_buffers.push_back(contreeHeaderBuffer);
    depthPass->readonly_storage_buffers.push_back(contreeDataBuffer);
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
    primaryPass->readonly_storage_buffers.push_back(contreeHeaderBuffer);
    primaryPass->readonly_storage_buffers.push_back(contreeDataBuffer);
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
    traceProbeLightingPass->readonly_storage_buffers.push_back(contreeHeaderBuffer);
    traceProbeLightingPass->readonly_storage_buffers.push_back(contreeDataBuffer);
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

    Input &input = GetModule<Input>();
    input.CreateAction("RegenLighting");
    input.CreateBinding("RegenLighting", SDLK_P);

    SynchronizeVoxelData();
    auto* uploads = renderer.CreateResource<VoxelUploadResource>();
    uploads->synchronize = [this] { SynchronizeVoxelData(); };
}

void VoxelRenderer::Process() {
    // Synchronization is executed by VoxelUploadResource before drawing.
}

void VoxelRenderer::SynchronizeVoxelData() {
    VoxelManager& vm = GetModule<VoxelManager>();

    if (vm.chunk_occupancy_needs_rebuild)
        vm.GenerateChunkOccupancyMap();

    pendingUploads.clear();
    UploadDirtyContreeNodes();
    UploadDirtyChunks();
    UploadDirtyOccupancyMap();
    UploadDirtyLightChunks();
    QueueUpload(cameraTransformBuffer, &cameraTransform, 0, sizeof(cameraTransform));

    FlushUploads();
    fullContreeUpload = false;
    fullChunkUpload = false;
}

void VoxelRenderer::Shutdown() {
    if (device) SDL_WaitForGPUIdle(device);
    if (uploadTransferBuffer) {
        SDL_ReleaseGPUTransferBuffer(device, uploadTransferBuffer);
        uploadTransferBuffer = nullptr;
        uploadTransferBufferSize = 0;
    }
    if (downloadTransferBuffer) {
        SDL_ReleaseGPUTransferBuffer(device, downloadTransferBuffer);
        downloadTransferBuffer = nullptr;
        downloadTransferBufferSize = 0;
    }
}

void VoxelRenderer::CreateLightChunks(bool defer_upload) {
    Renderer &renderer = GetModule<Renderer>();
    VoxelManager &vm = GetModule<VoxelManager>();
    if (vm.chunk_occupancy_needs_rebuild)
        vm.GenerateChunkOccupancyMap();
    if (lightChunksBuffer == nullptr) {
        lightChunksBuffer = renderer.CreateResource<TypedBuffer<LightChunk>>();
        lightChunksBuffer->usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
    }

    lightChunks.assign(vm.allocated_chunks.size(), LightChunk{});

    // Every neighbor's solid mask must be current before computing normals.
    for (size_t i = 0; i < lightChunks.size(); ++i) {
        lightChunks[i].position = vm.allocated_chunks[i].position;
        for (size_t p = 0; p < std::size(lightChunks[i].probes); ++p)
            UpdateLightProbeSolid(i, p);
    }
    for (size_t i = 0; i < lightChunks.size(); ++i) {
        for (size_t p = 0; p < std::size(lightChunks[i].probes); ++p)
            UpdateLightProbeNormal(i, p);
    }

    const size_t count = std::max<size_t>(1, lightChunks.size());
    if (lightChunksBuffer->GetSize() != count)
        lightChunksBuffer->SetSize(count);

    QueueUpload(lightChunksBuffer, lightChunks.data(), 0, lightChunks.size() * sizeof(LightChunk));
    if (!defer_upload) {
        FlushUploads();
    }
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

void VoxelRenderer::UpdateLightProbeNormal(uint32_t chunk_index, uint16_t probe_index) {
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

        neighbors[i] = lightChunks[neighborChunk].probes[neighborProbe].solid;
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



glm::ivec3 VoxelRenderer::GetNormal(uint64_t solid, const uint64_t neighbors[6]) {
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

    if (normal == glm::ivec3(0)) {
        normal = glm::ivec3(
            std::popcount(neighbors[1]) - std::popcount(neighbors[0]),
            std::popcount(neighbors[3]) - std::popcount(neighbors[2]),
            std::popcount(neighbors[5]) - std::popcount(neighbors[4])
        );
    }
    return glm::sign(normal);
}

void VoxelRenderer::EnsureUploadTransferBuffer(size_t required_size) {
    GrowTransferBuffer(device, uploadTransferBuffer, uploadTransferBufferSize,
                       required_size, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD);
}

void VoxelRenderer::QueueUpload(Buffer* destination, const void* source, size_t offset, size_t size) {
    if (size == 0)
        return;
    if (!destination->GetGPU() || offset > destination->GetSize() ||
        size > destination->GetSize() - offset)
        throw std::runtime_error("Invalid voxel upload destination");
    pendingUploads.push_back({destination, source, offset, size});
}

void VoxelRenderer::FlushUploads() {
    if (pendingUploads.empty())
        return;

    size_t total_size = 0;
    for (const auto& upload : pendingUploads) {
        if (upload.size > std::numeric_limits<Uint32>::max() - total_size)
            throw std::runtime_error("Voxel upload exceeds SDL's buffer size limit");
        total_size += upload.size;
    }
    EnsureUploadTransferBuffer(total_size);
    auto* mapped = static_cast<uint8_t*>(SDL_MapGPUTransferBuffer(device, uploadTransferBuffer, true));
    RequireGPUOperation(mapped != nullptr, "Map voxel upload buffer");
    size_t offset = 0;
    for (const auto& upload : pendingUploads) {
        memcpy(mapped + offset, upload.source, upload.size);
        offset += upload.size;
    }
    SDL_UnmapGPUTransferBuffer(device, uploadTransferBuffer);

    auto* cmd = SDL_AcquireGPUCommandBuffer(device);
    RequireGPUOperation(cmd != nullptr, "Acquire voxel upload command buffer");
    auto* pass = SDL_BeginGPUCopyPass(cmd);
    if (!pass) {
        SDL_CancelGPUCommandBuffer(cmd);
        RequireGPUOperation(false, "Begin voxel upload copy pass");
    }
    offset = 0;
    for (const auto& upload : pendingUploads) {
        // Partial writes must preserve all untouched elements in the GPU buffer.
        upload.destination->Upload(pass, uploadTransferBuffer, offset,
                                   upload.destination_offset, upload.size);
        offset += upload.size;
    }
    SDL_EndGPUCopyPass(pass);
    RequireGPUOperation(SDL_SubmitGPUCommandBuffer(cmd), "Submit voxel uploads");
    pendingUploads.clear();
}

void VoxelRenderer::UploadDirtyContreeNodes() {
    VoxelManager& vm = GetModule<VoxelManager>();

    if (contreeHeaderBuffer->GetSize() < vm.contree_headers.size()) {
        fullContreeUpload = true;
        contreeHeaderBuffer->SetSize(vm.contree_headers.capacity());
    }
    if (contreeDataBuffer->GetSize() < vm.contree_data.size()) {
        fullContreeUpload = true;
        contreeDataBuffer->SetSize(vm.contree_data.capacity());
    }

    auto queue = [&](size_t first, size_t count) {
        if (count == 0) return;
        QueueUpload(contreeHeaderBuffer, vm.contree_headers.data() + first,
                    first * sizeof(ContreeHeader), count * sizeof(ContreeHeader));
        QueueUpload(contreeDataBuffer, vm.contree_data.data() + first,
                    first * sizeof(ContreeData), count * sizeof(ContreeData));
    };
    if (fullContreeUpload)
        queue(0, vm.contree_headers.size());
    else
        VisitRanges(vm.dirty_contree_indices, queue);
}

void VoxelRenderer::UploadDirtyChunks() {
    VoxelManager& vm = GetModule<VoxelManager>();

    const size_t count = vm.allocated_chunks.size();
    if (chunks->GetSize() < count) {
        fullChunkUpload = true;
        chunks->SetSize(vm.allocated_chunks.capacity());
    }

    auto queue = [&](size_t first, size_t range_count) {
        if (range_count == 0) return;
        QueueUpload(chunks, vm.allocated_chunks.data() + first,
                    first * sizeof(Chunk), range_count * sizeof(Chunk));
    };
    if (fullChunkUpload) {
        queue(0, count);
        return;
    }

    std::vector<uint32_t> indices;
    indices.reserve(vm.dirty_chunk_positions.size());
    for (const glm::ivec3& position : vm.dirty_chunk_positions) {
        uint32_t index = vm.GetChunkIndex(position);
        if (index != POINTER_EMPTY)
            indices.push_back(index);
    }
    VisitRanges(indices, queue);
}

void VoxelRenderer::UploadDirtyOccupancyMap() {
    VoxelManager& vm = GetModule<VoxelManager>();

    if (vm.chunk_occupancy_needs_rebuild) {
        vm.GenerateChunkOccupancyMap();
    }
    if (!vm.chunk_occupancy_dirty && !fullChunkUpload)
        return;

    occupancyUploadHeader = {};
    occupancyUploadHeader.position = vm.chunk_occupancy.position;
    occupancyUploadHeader.size = vm.chunk_occupancy.size;

    const size_t count = vm.chunk_occupancy.get_size();
    if (chunkPositions->GetSize() < count)
        chunkPositions->SetSize(count);
    QueueUpload(chunkPositions, vm.chunk_occupancy.chunks, 0, count * sizeof(uint32_t));
    QueueUpload(chunkPositionsHeader, &occupancyUploadHeader, 0, sizeof(occupancyUploadHeader));
}

void VoxelRenderer::DownloadLightChunks(const std::vector<uint32_t>& indices) {
    if (indices.empty()) return;

    const size_t required = indices.size() * sizeof(LightChunk);
    GrowTransferBuffer(device, downloadTransferBuffer, downloadTransferBufferSize,
                       required, SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD);
    auto* cmd = SDL_AcquireGPUCommandBuffer(device);
    RequireGPUOperation(cmd != nullptr, "Acquire light readback command buffer");
    auto* pass = SDL_BeginGPUCopyPass(cmd);
    if (!pass) {
        SDL_CancelGPUCommandBuffer(cmd);
        RequireGPUOperation(false, "Begin light readback copy pass");
    }
    for (size_t i = 0; i < indices.size(); ++i)
        lightChunksBuffer->Download(pass, downloadTransferBuffer, i * sizeof(LightChunk), indices[i], 1);
    SDL_EndGPUCopyPass(pass);
    auto* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    RequireGPUOperation(fence != nullptr, "Submit light readback");
    const bool ready = SDL_WaitForGPUFences(device, true, &fence, 1);
    SDL_ReleaseGPUFence(device, fence);
    RequireGPUOperation(ready, "Wait for light readback");

    auto* mapped = static_cast<uint8_t*>(SDL_MapGPUTransferBuffer(device, downloadTransferBuffer, false));
    RequireGPUOperation(mapped != nullptr, "Map light readback buffer");
    for (size_t i = 0; i < indices.size(); ++i)
        memcpy(&lightChunks[indices[i]], mapped + i * sizeof(LightChunk), sizeof(LightChunk));
    SDL_UnmapGPUTransferBuffer(device, downloadTransferBuffer);
}

void VoxelRenderer::UploadDirtyLightChunks() {
    VoxelManager& vm = GetModule<VoxelManager>();
    if (vm.chunk_occupancy_dirty ||
        GetModule<Input>().IsPressed("RegenLighting")) {
        CreateLightChunks(true);
        return;
    }

    const glm::ivec3 offsets[] = {
        {0, 0, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
        {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    };
    std::vector<uint32_t> indices;
    for (const auto& position : vm.dirty_chunk_positions) {
        for (const auto& offset : offsets) {
            uint32_t index = vm.GetChunkIndex(position + offset);
            if (index != POINTER_EMPTY)
                indices.push_back(index);
        }
    }
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    DownloadLightChunks(indices);

    // Two passes avoid using a neighbor's old solid mask for boundary normals.
    for (uint32_t index : indices) {
        for (size_t p = 0; p < std::size(lightChunks[index].probes); ++p)
            UpdateLightProbeSolid(index, p);
    }
    for (uint32_t index : indices) {
        for (size_t p = 0; p < std::size(lightChunks[index].probes); ++p)
            UpdateLightProbeNormal(index, p);
    }
    VisitRanges(indices, [&](size_t first, size_t count) {
        QueueUpload(lightChunksBuffer, lightChunks.data() + first,
                    first * sizeof(LightChunk), count * sizeof(LightChunk));
    });
}
