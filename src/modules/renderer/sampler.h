#pragma once

#include "resource.h"

#include "texture.h"

class Sampler : Resource<SDL_GPUSampler>  {
    public:
        using Resource::Resource;
        ~Sampler() override;

        void Create() override;
        void Destroy() override;

        SDL_GPUSampler *GetGPU(void) override;
};

class SamplerTextureBinding {
    public:
        SamplerTextureBinding(Sampler *sampler, Texture *texture);
        Sampler *sampler;
        Texture *texture;
};