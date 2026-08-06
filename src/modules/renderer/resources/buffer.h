#pragma once

#include <SDL3/SDL_gpu.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "../resource.h"

class Buffer : public Resource<SDL_GPUBuffer> {
    public:
        using Resource::Resource;
        ~Buffer() override;

        void Create(void) override;
        void Destroy(void) override;
        SDL_GPUBuffer* GetGPU(void) override;

        void Upload(void *source, size_t cpu_start, size_t gpu_start, size_t size);
        void Download(void *dest, size_t cpu_start, size_t gpu_start, size_t size);

        void SetSize(size_t size);
        size_t GetSize();

        size_t size = 0;
        SDL_GPUBufferUsageFlags usage = 0;
};

template<typename T>
class TypedBuffer : public Buffer {
    public:
        using Buffer::Buffer;

        using Buffer::Create;
        using Buffer::Upload;
        using Buffer::Download;
        using Buffer::SetSize;
        using Buffer::GetSize;

        void Upload(const T *data, size_t count, size_t elementOffset = 0) {
            Buffer::Upload((void*)data, 0, elementOffset * sizeof(T), count * sizeof(T));
        }

        void Upload(const std::vector<T> &data, size_t elementOffset = 0) {
            Upload(data.data(), data.size(), elementOffset);
        }

        void Download(T *out, size_t count, size_t elementOffset = 0) {
            Buffer::Download((void*)out, 0, elementOffset * sizeof(T), count * sizeof(T));
        }

        void Download(std::vector<T> &out, size_t elementOffset = 0) {
            Download(out.data(), out.size(), elementOffset);
        }

        void SetSize(size_t elementCount) {
            Buffer::SetSize(sizeof(T) * elementCount);
        }
        size_t GetSize() {
            return sizeof(T) * Buffer::GetSize();
        }
};