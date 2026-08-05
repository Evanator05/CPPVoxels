#pragma  once
#include "SDL3/SDL_gpu.h"

class IResource {
    public:
        IResource(SDL_GPUDevice* device)
        : device(device) {}
        virtual ~IResource() = default;
        virtual void Create() = 0;
        virtual void Destroy() = 0;
    protected:
        SDL_GPUDevice *device = nullptr;
};

template<typename T>
class Resource : public IResource {
    public:
        Resource(SDL_GPUDevice* device)
        : IResource(device) {}
        virtual ~Resource() = default;
        virtual void Create() = 0;
        virtual void Destroy() = 0;
        virtual T *GetGPU() = 0;
    protected:
        T *gpu_resource = nullptr;
};