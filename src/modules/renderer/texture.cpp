#include "texture.h"

#include <stdexcept>

Texture::Texture(SDL_GPUDevice *device, glm::ivec2 size, SDL_GPUTextureUsageFlags usage, SDL_GPUTextureFormat format) : Resource(device) {
    this->size = size;
    this->usage = usage;
    this->format = format;
}

Texture::~Texture() {
    Destroy();
}

void Texture::CreateFrom(SDL_GPUTexture *texture, glm::ivec2 size, SDL_GPUTextureUsageFlags usage, SDL_GPUTextureFormat format) {
    Destroy();
    owned = false;
    gpu_resource = texture;
    this->size = size;
    this->usage = usage;
    this->format = format;
    
}

void Texture::Create() {
    Destroy();
    owned = true;
    SDL_GPUTextureCreateInfo createInfo{};
    createInfo.width = (Uint32)size.x;
    createInfo.height = (Uint32)size.y;
    createInfo.layer_count_or_depth = 1;
    createInfo.num_levels = 1;
    createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
    createInfo.format = format;
    createInfo.usage = usage;
    gpu_resource = SDL_CreateGPUTexture(device, &createInfo);
    if (!gpu_resource) throw std::runtime_error(SDL_GetError());
}

void Texture::Destroy() {
    if (!owned) return;
    if (!gpu_resource) return;
    SDL_ReleaseGPUTexture(device, gpu_resource);
    gpu_resource = nullptr;
}

SDL_GPUTexture* Texture::GetGPU() {
    return gpu_resource;
}