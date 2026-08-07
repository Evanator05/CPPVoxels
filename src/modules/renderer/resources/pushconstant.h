#pragma once

#include "../resource.h"

template <typename T>
class PushConstant : public IExecutableResource {
    public:
        using IExecutableResource::IExecutableResource;
        void Create(void) override;
        void Destroy(void) override;
        
        void Execute(SDL_GPUCommandBuffer *cmd) override {
            SDL_PushGPUComputeUniformData(cmd, 0, &value, sizeof(T));
        }

        T value;
};