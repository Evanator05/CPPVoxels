#pragma once

#include "../shaderpass.h"
#include "functional"

class FunctionalPass : public ShaderPass {
    public:
        using ShaderPass::ShaderPass;

        using Function = std::function<void(SDL_GPUCommandBuffer*)>;

        void Create(void) override;
        void Destroy(void) override;
        void Execute(SDL_GPUCommandBuffer* cmd) override;

        Function function;
};