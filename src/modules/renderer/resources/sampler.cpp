#include "sampler.h"

Sampler::~Sampler() {
    Destroy();
}

void Sampler::Create() {
    Destroy();
    SDL_GPUSamplerCreateInfo createInfo{};

    gpu_resource = SDL_CreateGPUSampler(device, &createInfo);
}

void Sampler::Destroy() {
    if (gpu_resource) SDL_ReleaseGPUSampler(device, gpu_resource);
}

SDL_GPUSampler* Sampler::GetGPU() {
    return gpu_resource;
}

SamplerTextureBinding::SamplerTextureBinding(Sampler *sampler, Texture *texture) {
    this->sampler = sampler;
    this->texture = texture;
}