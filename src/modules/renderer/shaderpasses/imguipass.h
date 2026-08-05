#pragma once

#include "../shaderpass.h"
#include "../resources/texture.h"

#include "gui.h"

class ImGuiPass : public ShaderPass {
    public:
        using ShaderPass::ShaderPass;

        void Create(void) override;
        void Destroy(void) override;
        void Execute(SDL_GPUCommandBuffer* cmd) override;

        Texture *destination;

    private:
};