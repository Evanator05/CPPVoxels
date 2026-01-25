#pragma once

#include <vector>
#include "SDL3/SDL_gpu.h"
#include "glm/vec3.hpp"

#include "allocator.h"

#define GPUBUFFERCOUNT 5

typedef struct _BufferSizes {
    alignas(16) glm::ivec3 chunkOccupancyMin;
    alignas(16) glm::ivec3 chunkOccupancyMax;
    int voxelsSize;
    int chunksSize;
} BufferSizes;

typedef struct _ChunkVisiblility {
    uint32_t data[128]; 
} ChunkVisiblility;

void gpubuffers_init(void);

void gpubuffers_cleanup(void);

void gpubuffers_upload(void);

SDL_GPUBuffer** gpubuffers_getVoxelBuffers(void);

void gpubuffers_createBuffers(void);

void gpubuffers_startUpload(void);
void gpubuffers_finishUpload(void);

template <typename T>
void gpubuffers_createBufferFromAllocator(Allocator<T> &data, SDL_GPUBuffer *&buffer, SDL_GPUTransferBuffer *&transferBuffer);

template <typename T>
void gpubuffers_createBufferFromVector(std::vector<T> &data, SDL_GPUBuffer *&buffer, SDL_GPUTransferBuffer *&transferBuffer);

template <typename T>
void gpubuffers_uploadBufferFromAllocator(Allocator<T> &data, SDL_GPUBuffer *&buffer, SDL_GPUTransferBuffer *&transferBuffer);

template <typename T>
void gpubuffers_uploadBufferFromVector(std::vector<T> &data, SDL_GPUBuffer *&buffer, SDL_GPUTransferBuffer *&transferBuffer);

void gpubuffers_uploadSizesBuffer(void);

void gpubuffers_createSizesBuffer(void);
void gpubuffers_createVoxelBuffer(void);
void gpubuffers_createChunkBuffer(void);
void gpubuffers_createOccupancyBuffer(void);
void gpubuffers_createVisiblilityBuffer(void);