#include "buffer.h"

Buffer::~Buffer() {
    Destroy();
}

void Buffer::Create() {
    Destroy();

    // create buffer
    SDL_GPUBufferCreateInfo bci{};
    bci.size = size;
    bci.props = 0;
    bci.usage = usage; 
    gpu_resource = SDL_CreateGPUBuffer(device, &bci);
}

void Buffer::Destroy() {
    if (gpu_resource) {
        SDL_ReleaseGPUBuffer(device, gpu_resource);
    } 
}

void Buffer::Upload(SDL_GPUCopyPass *pass, void *source, size_t cpu_start, size_t gpu_start, size_t size) {
    
    SDL_GPUTransferBufferCreateInfo tbci{};
    tbci.size = size;
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    SDL_GPUTransferBuffer *transferBuffer = SDL_CreateGPUTransferBuffer(device, &tbci);

    void *mapped = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
    memcpy(mapped+gpu_start, source+cpu_start, size);
    SDL_UnmapGPUTransferBuffer(device, transferBuffer);

    SDL_GPUTransferBufferLocation *tsource{};
    tsource->transfer_buffer = transferBuffer;
    tsource->offset = cpu_start;

    SDL_GPUBufferRegion  *tdestination{};
    tdestination->offset = gpu_start;
    tdestination->size = size;
    tdestination->buffer = gpu_resource;
    SDL_UploadToGPUBuffer(pass, tsource, tdestination, false);
    
    SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
}

void Buffer::Download(SDL_GPUCopyPass *pass, void *dest, size_t cpu_start, size_t gpu_start, size_t size) {
    SDL_GPUTransferBufferCreateInfo tbci{};
    tbci.size = size;
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    SDL_GPUTransferBuffer *transferBuffer = SDL_CreateGPUTransferBuffer(device, &tbci);

    SDL_GPUBufferRegion *tsource{};
    tsource->buffer = gpu_resource;
    tsource->offset = cpu_start;
    tsource->size = size;
    
    SDL_GPUTransferBufferLocation *tdestination{};
    tdestination->offset = gpu_start;
    tdestination->transfer_buffer = transferBuffer;

    SDL_DownloadFromGPUBuffer(pass, tsource, tdestination);

    void *mapped = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
    memcpy(dest+cpu_start, mapped, size);
    SDL_UnmapGPUTransferBuffer(device, transferBuffer);
}

SDL_GPUBuffer* Buffer::GetGPU() {
    return gpu_resource;
}