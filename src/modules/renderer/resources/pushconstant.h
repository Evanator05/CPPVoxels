#pragma once

#include "../resource.h"

template <typename T>
class PushConstant : public IExecutableResource {
    public:
        using IExecutableResource::IExecutableResource;
        void Create(void) override;
        void Destroy(void) override;
        
        void Execute(SDL_GPUCommandBuffer *cmd) override {
            SDL_PushGPUComputeUniformData(cmd, slot, &value, sizeof(T));
        }
        uint32_t slot;
        T value;
};

template<typename T>
void PushConstant<T>::Create()
{
}

template<typename T>
void PushConstant<T>::Destroy()
{
}