#include "resource.h"
class Buffer : Resource<SDL_GPUBuffer> {
    public:
        using Resource::Resource;
        ~Buffer() override;
        void Create(void) override;
        void Destroy(void) override;

        void Upload(SDL_GPUCopyPass *pass, void *source, size_t cpu_start, size_t gpu_start, size_t size);
        void Download(SDL_GPUCopyPass *pass, void *dest, size_t cpu_start, size_t gpu_start, size_t size);

        SDL_GPUBuffer* GetGPU(void) override;

        size_t size = 0;
        SDL_GPUBufferUsageFlags usage = 0;
};