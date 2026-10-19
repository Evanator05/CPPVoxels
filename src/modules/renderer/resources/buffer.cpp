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

void Buffer::Upload(SDL_GPUCopyPass *pass, SDL_GPUTransferBuffer *transfer_buffer, size_t transfer_start, size_t gpu_start, size_t size) {
    SDL_GPUTransferBufferLocation source{};
    source.transfer_buffer = transfer_buffer;
    source.offset = static_cast<Uint32>(transfer_start);

    SDL_GPUBufferRegion destination{};
    destination.buffer = gpu_resource;
    destination.offset = static_cast<Uint32>(gpu_start);
    destination.size = static_cast<Uint32>(size);

    SDL_UploadToGPUBuffer(pass, &source, &destination, false);
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

void Buffer::Download(
    SDL_GPUCopyPass *pass,
    SDL_GPUTransferBuffer *transfer_buffer,
    size_t transfer_start,
    size_t gpu_start,
    size_t size
) {
    SDL_GPUBufferRegion source{};
    source.buffer = gpu_resource;
    source.offset = static_cast<Uint32>(gpu_start);
    source.size = static_cast<Uint32>(size);

    SDL_GPUTransferBufferLocation destination{};
    destination.transfer_buffer = transfer_buffer;
    destination.offset = static_cast<Uint32>(transfer_start);

    SDL_DownloadFromGPUBuffer(pass, &source, &destination);
}

void Buffer::Download(
    void *dest,
    size_t cpu_start,
    size_t gpu_start,
    size_t size
) {
    if (size == 0)
        return;

    SDL_GPUTransferBufferCreateInfo tbci{};
    tbci.size = static_cast<Uint32>(size);
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;

    SDL_GPUTransferBuffer *transferBuffer =
        SDL_CreateGPUTransferBuffer(device, &tbci);

    if (!transferBuffer)
        return;

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);

    if (!cmd) {
        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
        return;
    }

    SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(cmd);

    if (!pass) {
        SDL_CancelGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
        return;
    }

    Download(pass, transferBuffer, 0, gpu_start, size);

    SDL_EndGPUCopyPass(pass);

    SDL_GPUFence *fence =
        SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);

    if (!fence) {
        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
        return;
    }

    if (SDL_WaitForGPUFences(device, true, &fence, 1)) {
        uint8_t *mapped = static_cast<uint8_t*>(
            SDL_MapGPUTransferBuffer(device, transferBuffer, false)
        );

        if (mapped) {
            memcpy(static_cast<uint8_t*>(dest) + cpu_start, mapped, size);
            SDL_UnmapGPUTransferBuffer(device, transferBuffer);
        }
    }

    SDL_ReleaseGPUFence(device, fence);
    SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
}

void Buffer::SetSize(size_t size) {
    this->size = size;
    Create();
}

size_t Buffer::GetSize() {
    return this->size;
}