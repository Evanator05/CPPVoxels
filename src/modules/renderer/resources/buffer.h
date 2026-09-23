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

        void Upload(SDL_GPUCopyPass *pass, SDL_GPUTransferBuffer *transfer_buffer, size_t transfer_start, size_t gpu_start, size_t size);
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

        void Upload(const T *data, size_t index, size_t count) {
            Buffer::Upload(
                (void*)data,
                index * sizeof(T),
                index * sizeof(T),
                count * sizeof(T)
            );
        }

        void Upload(SDL_GPUCopyPass *pass, SDL_GPUTransferBuffer *transfer_buffer, size_t transfer_start, size_t index, size_t count) {
            Buffer::Upload(pass, transfer_buffer, transfer_start, index * sizeof(T), count * sizeof(T));
        }

        void Upload(const std::vector<T> &data, size_t index, size_t count) {
            Upload(data.data(), index, count);
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
            return Buffer::GetSize() / sizeof(T);
        }
};