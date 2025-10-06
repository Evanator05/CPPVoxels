#include <iostream>
#include <stdexcept>

#include "i_gpubuffers.h"
#include "voxel.h"
#include "i_graphics.h"
#include "worldinfo.h"
#include "chunkbvh.h"

SDL_GPUBuffer *sizesBuffer = nullptr;
SDL_GPUTransferBuffer *sizesTransferBuffer = nullptr;

SDL_GPUBuffer *voxelBuffer = nullptr;
SDL_GPUTransferBuffer *voxelTransferBuffer = nullptr;

SDL_GPUBuffer *chunkBuffer = nullptr;
SDL_GPUTransferBuffer *chunkTransferBuffer = nullptr;

SDL_GPUCommandBuffer *gpubuffers_cmd = nullptr;
SDL_GPUCopyPass *gpubuffers_cpy = nullptr;

void gpubuffers_init() {
    gpubuffers_createBuffers();
}

void gpubuffers_createBuffers() {
    gpubuffers_createSizesBuffer();
    gpubuffers_createVoxelBuffer();
    gpubuffers_createChunkBuffer();
}

void gpubuffers_cleanup() {
    SDL_GPUDevice *device = graphics_getDevice();

    SDL_ReleaseGPUBuffer(device, sizesBuffer);
    SDL_ReleaseGPUBuffer(device, voxelBuffer);
    SDL_ReleaseGPUBuffer(device, chunkBuffer);

    SDL_ReleaseGPUTransferBuffer(device, sizesTransferBuffer);
    SDL_ReleaseGPUTransferBuffer(device, voxelTransferBuffer);
    SDL_ReleaseGPUTransferBuffer(device, chunkTransferBuffer);
}

void gpubuffers_upload() {
    gpubuffers_startUpload();
    gpubuffers_uploadSizesBuffer();
    gpubuffers_uploadBufferFromAllocator(voxelData, voxelBuffer, voxelTransferBuffer);
    gpubuffers_uploadBufferFromAllocator(chunkData, chunkBuffer, chunkTransferBuffer);
    gpubuffers_finishUpload();
}

void gpubuffers_startUpload() {
    SDL_GPUDevice *device = graphics_getDevice();
    gpubuffers_cmd = SDL_AcquireGPUCommandBuffer(device);
    gpubuffers_cpy = SDL_BeginGPUCopyPass(gpubuffers_cmd);
}

void gpubuffers_finishUpload() {
    SDL_EndGPUCopyPass(gpubuffers_cpy);
    SDL_SubmitGPUCommandBuffer(gpubuffers_cmd);
}

template <typename T>
void gpubuffers_uploadBufferFromAllocator(Allocator<T> &data, SDL_GPUBuffer *&buffer, SDL_GPUTransferBuffer *&transferBuffer) {
    if (data.size() == 0) return;

    SDL_GPUDevice *device = graphics_getDevice();

    // copy voxel data into transfer buffer
    void *ptr = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
    T *voxelPtr = (T*)ptr;
    memcpy(voxelPtr, data.data(), data.size()*sizeof(T));
    SDL_UnmapGPUTransferBuffer(device, transferBuffer);

    // create transfer buffer location
    SDL_GPUTransferBufferLocation tbl{};
    tbl.transfer_buffer = transferBuffer;
    tbl.offset = 0;

    // create transfer buffer region
    SDL_GPUBufferRegion br{};
    br.buffer = buffer;
    br.offset = 0;
    br.size = data.size()*sizeof(T);

    // upload to the buffer
    SDL_UploadToGPUBuffer(gpubuffers_cpy, &tbl, &br, false);
}


template <typename T>
void gpubuffers_createBufferFromAllocator(Allocator<T> &data, SDL_GPUBuffer *&buffer, SDL_GPUTransferBuffer *&transferBuffer) {
    SDL_GPUDevice *device = graphics_getDevice();

    // if buffers already exist release them
    if (buffer) SDL_ReleaseGPUBuffer(device, buffer);
    if (transferBuffer) SDL_ReleaseGPUTransferBuffer(device, transferBuffer);

    // create buffer
    SDL_GPUBufferCreateInfo bci{};
    bci.size = data.size()*sizeof(T);
    bci.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    buffer = SDL_CreateGPUBuffer(device, &bci);

    if (!buffer) std::cerr << "Failed to create buffer\n";

    // create transfer buffer
    SDL_GPUTransferBufferCreateInfo tbci{};
    tbci.size = data.size()*sizeof(T);
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferBuffer = SDL_CreateGPUTransferBuffer(device, &tbci);
    
    if (!transferBuffer) std::cerr << "Failed to create transfer buffer\n";
}

void gpubuffers_createSizesBuffer() {
    SDL_GPUDevice *device = graphics_getDevice();

    // if buffers already exist release them
    if (sizesBuffer) SDL_ReleaseGPUBuffer(device, sizesBuffer);
    if (sizesTransferBuffer) SDL_ReleaseGPUTransferBuffer(device, sizesTransferBuffer);

    // create buffer
    SDL_GPUBufferCreateInfo bci{};
    bci.size = sizeof(BufferSizes);
    bci.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    sizesBuffer = SDL_CreateGPUBuffer(device, &bci);

    if (!sizesBuffer) std::cerr << "Failed to create sizes buffer\n";

    // create transfer buffer
    SDL_GPUTransferBufferCreateInfo tbci{};
    tbci.size = sizeof(BufferSizes);
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    sizesTransferBuffer = SDL_CreateGPUTransferBuffer(device, &tbci);
    
    if (!sizesTransferBuffer) std::cerr << "Failed to create sizes transfer buffer\n";
}

void gpubuffers_uploadSizesBuffer() {
    SDL_GPUDevice *device = graphics_getDevice();

    // copy voxel data into transfer buffer
    void *ptr = SDL_MapGPUTransferBuffer(device, sizesTransferBuffer, false);
    BufferSizes sizes{};
    sizes.voxelsSize = voxelData.size();
    sizes.chunksSize = chunkData.size();
    memcpy(ptr, &sizes, sizeof(BufferSizes));
    SDL_UnmapGPUTransferBuffer(device, sizesTransferBuffer);

    // create transfer buffer location
    SDL_GPUTransferBufferLocation tbl{};
    tbl.transfer_buffer = sizesTransferBuffer;
    tbl.offset = 0;

    // create transfer buffer region
    SDL_GPUBufferRegion br{};
    br.buffer = sizesBuffer;
    br.offset = 0;
    br.size = sizeof(BufferSizes);

    // upload to the buffer
    SDL_UploadToGPUBuffer(gpubuffers_cpy, &tbl, &br, false);
}

void gpubuffers_createVoxelBuffer() {
    gpubuffers_createBufferFromAllocator(voxelData, voxelBuffer, voxelTransferBuffer);
}

void gpubuffers_createChunkBuffer() {
    gpubuffers_createBufferFromAllocator(chunkData, chunkBuffer, chunkTransferBuffer);
}

SDL_GPUBuffer** gpubuffers_getVoxelBuffers() {
    static SDL_GPUBuffer* buffers[GPUBUFFERCOUNT];
    buffers[0] = worldInfoBuffer;
    buffers[1] = sizesBuffer;
    buffers[2] = voxelBuffer;
    buffers[3] = chunkBuffer;
    return buffers;
}