#include "buffer.h"

Buffer::~Buffer() {
    Destroy();
}

void Buffer::Create() {
    Destroy();

    SDL_GPUBufferCreateInfo bci{};
    bci.size = static_cast<Uint32>(size);
    bci.props = 0;
    bci.usage = usage;

    gpu_resource = SDL_CreateGPUBuffer(device, &bci);
}

void Buffer::Destroy() {
    if (gpu_resource) {
        SDL_ReleaseGPUBuffer(device, gpu_resource);
        gpu_resource = nullptr;
    }
}

SDL_GPUBuffer* Buffer::GetGPU() {
    return gpu_resource;
}

void Buffer::Upload(void *source, size_t cpu_start, size_t gpu_start, size_t size) {
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTransferBufferCreateInfo tbci{};
    tbci.size = static_cast<Uint32>(size);
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    SDL_GPUTransferBuffer *transferBuffer = SDL_CreateGPUTransferBuffer(device, &tbci);

    uint8_t *mapped = (uint8_t*)SDL_MapGPUTransferBuffer(device, transferBuffer, false);
    memcpy(mapped, (uint8_t*)source + cpu_start, size);
    SDL_UnmapGPUTransferBuffer(device, transferBuffer);

    SDL_GPUTransferBufferLocation tsource{};
    tsource.transfer_buffer = transferBuffer;
    tsource.offset = 0;

    SDL_GPUBufferRegion tdestination{};
    tdestination.buffer = gpu_resource;
    tdestination.offset = static_cast<Uint32>(gpu_start);
    tdestination.size = static_cast<Uint32>(size);

    SDL_UploadToGPUBuffer(pass, &tsource, &tdestination, false);

    SDL_ReleaseGPUTransferBuffer(device, transferBuffer);

    SDL_EndGPUCopyPass(pass);
    SDL_SubmitGPUCommandBuffer(cmd);
}

void Buffer::Download(void *dest, size_t cpu_start, size_t gpu_start, size_t size) {
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTransferBufferCreateInfo tbci{};
    tbci.size = static_cast<Uint32>(size);
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    SDL_GPUTransferBuffer *transferBuffer = SDL_CreateGPUTransferBuffer(device, &tbci);

    SDL_GPUBufferRegion tsource{};
    tsource.buffer = gpu_resource;
    tsource.offset = static_cast<Uint32>(gpu_start);
    tsource.size = static_cast<Uint32>(size);

    SDL_GPUTransferBufferLocation tdestination{};
    tdestination.transfer_buffer = transferBuffer;
    tdestination.offset = 0;

    SDL_DownloadFromGPUBuffer(pass, &tsource, &tdestination);

    uint8_t *mapped = (uint8_t*)SDL_MapGPUTransferBuffer(device, transferBuffer, false);
    memcpy((uint8_t*)dest + cpu_start, mapped, size);
    SDL_UnmapGPUTransferBuffer(device, transferBuffer);

    SDL_ReleaseGPUTransferBuffer(device, transferBuffer);

    SDL_EndGPUCopyPass(pass);
    SDL_SubmitGPUCommandBuffer(cmd);
}

void Buffer::SetSize(size_t size) {
    this->size = size;
    Create();
}

size_t Buffer::GetSize() {
    return this->size;
}