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
    Relptr<AllocatedChunksBase> chunk_index = vm.AllocateChunk(glm::ivec3(0, 0, 0));
    vm.GenerateChunkOccupancyMap();
    Voxel v{};
    v.set_r(0);
    v.set_g(31);
    v.set_b(31);
    v.set_solid(true);
    
    vm.AllocateChunk(glm::ivec3(1, 1, 1));
    vm.AllocateChunk(glm::ivec3(2, 2, 2));
    vm.AllocateChunk(glm::ivec3(3, 3, 3));
    vm.AllocateChunk(glm::ivec3(1, 2, 2));
    vm.GenerateChunkOccupancyMap();
    vm.FillVoxels(glm::ivec3(0), glm::ivec3(4), v);
    v.set_r(0);
    v.set_g(31);
    v.set_b(0);
    vm.FillVoxels(glm::ivec3(16), glm::ivec3(42), v);
    v.set_r(31);
    v.set_g(0);
    v.set_b(0);
    v.set_solid(true);
    vm.FillVoxels(glm::ivec3(45), glm::ivec3(256), v);
    vm.SetVoxel(glm::ivec3(16, 18, 24), v);
    vm.SetVoxel(glm::ivec3(16, 22, 16), v);
    vm.SetVoxel(glm::ivec3(9, 5, 2), v);
    vm.SetVoxel(glm::ivec3(2, 2, 8), v);
    GetModule<Console>().Log(vm.DumpContreeGraph(0), Console::LogLevel::Info);

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
        glm::ivec2 size = w.GetSize();
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
    depthUpscale->readonly_storage_buffers.push_back(nodes);
    depthUpscale->readonly_storage_buffers.push_back(chunks);
    depthUpscale->readonly_storage_buffers.push_back(chunkPositionsHeader);
    depthUpscale->readonly_storage_buffers.push_back(chunkPositions);
    depthUpscale->readonly_storage_buffers.push_back(posBuffer);
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
}

void VoxelRenderer::Shutdown() {
    
}
