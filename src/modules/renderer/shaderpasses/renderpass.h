#pragma once

#include "../shaderpass.h"
#include "../resources/texture.h"
#include "../resources/buffer.h"

class RenderPass : public ShaderPass {
    public:
        void Create(void) override;
        void Destroy(void) override;
        void Execute(SDL_GPUCommandBuffer* cmd) override;

        static constexpr uint8_t INDEXED = 0b00000001;
        static constexpr uint8_t INDIRECT = 0b00000010;

        Texture *output = nullptr;
        Buffer *vertices = nullptr;
        
    private:
        SDL_GPUGraphicsPipeline *pipeline = nullptr;
};