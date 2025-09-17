#include "worldinfo.h"
#include "i_graphics.h"

#include <stdio.h>

WorldInfo worldInfo;
SDL_GPUBuffer *worldInfoBuffer;
SDL_GPUTransferBuffer *worldInfoTransferBuffer;

void worldInfo_CreateBuffers() {
    SDL_GPUDevice *device = graphics_getDevice();
    // create buffer
    SDL_GPUBufferCreateInfo bci{};
    bci.size = sizeof(WorldInfo);
    bci.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    worldInfoBuffer = SDL_CreateGPUBuffer(device, &bci);

    // create transfer buffer
    SDL_GPUTransferBufferCreateInfo tbci{};
    tbci.size = sizeof(WorldInfo);
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    worldInfoTransferBuffer = SDL_CreateGPUTransferBuffer(device, &tbci);
}

void worldInfo_TransforToGPU() {
    SDL_GPUDevice *device = graphics_getDevice();

    // copy voxel data into transfer buffer
    WorldInfo *ptr = (WorldInfo*)SDL_MapGPUTransferBuffer(device, worldInfoTransferBuffer, false);
    memcpy(ptr, &worldInfo, sizeof(WorldInfo));
    SDL_UnmapGPUTransferBuffer(device, worldInfoTransferBuffer);

    printf("%f %f %f %f %f %f\n", worldInfo.cameraPos.x, worldInfo.cameraPos.y, worldInfo.cameraPos.z, worldInfo.cameraRot.x, worldInfo.cameraRot.y, worldInfo.time);

    // create transfer buffer location
    SDL_GPUTransferBufferLocation tbl{};
    tbl.transfer_buffer = worldInfoTransferBuffer;
    tbl.offset = 0;

    // create transfer buffer region
    SDL_GPUBufferRegion br{};
    br.buffer = worldInfoBuffer;
    br.offset = 0;
    br.size = sizeof(WorldInfo);


    SDL_GPUCommandBuffer *gpubuffers_cmd = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass *gpubuffers_cpy = SDL_BeginGPUCopyPass(gpubuffers_cmd);
    // upload to the buffer
    SDL_UploadToGPUBuffer(gpubuffers_cpy, &tbl, &br, false);

    SDL_EndGPUCopyPass(gpubuffers_cpy);
    SDL_SubmitGPUCommandBuffer(gpubuffers_cmd);
}

void worldInfo_init() {
    worldInfo_CreateBuffers();
}

void worldInfo_update() {
    worldInfo.time += 0.006;
    worldInfo_TransforToGPU();
}

void worldInfo_cleanup(void) {
    SDL_GPUDevice *device = graphics_getDevice();
    SDL_ReleaseGPUBuffer(device, worldInfoBuffer);
    SDL_ReleaseGPUTransferBuffer(device, worldInfoTransferBuffer);
}