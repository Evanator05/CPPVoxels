#pragma  once
#include "SDL3/SDL_gpu.h"

template<typename T>
class Resource {
    public:
        Resource(SDL_GPUDevice* device)
        : device(device) {}
        virtual ~Resource() = default;
        virtual void Create() = 0;
        virtual void Destroy() = 0;
        virtual T *GetGPU() = 0;
    protected:
        SDL_GPUDevice *device = nullptr;
        T *gpu_resource = nullptr;
};