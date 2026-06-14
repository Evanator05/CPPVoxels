#pragma once

#include "SDL3/SDL_gpu.h"

class ShaderPass {
    public:
        ShaderPass(SDL_GPUDevice *device);
        ~ShaderPass();

        virtual void Create(void) = 0;
        virtual void Destroy(void);
        virtual void Execute(SDL_GPUCommandBuffer *cmd) = 0;
        
    private:
        
    protected:
        SDL_GPUDevice *device = nullptr;
};