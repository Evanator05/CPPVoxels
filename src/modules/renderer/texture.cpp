#include "texture.h"

#include <stdexcept>

Texture::Texture(SDL_GPUDevice *device) {
    this->device = device;
}

Texture::Texture(SDL_GPUDevice *device, glm::ivec2 size, SDL_GPUTextureUsageFlags usage, SDL_GPUTextureFormat format) : Texture(device) {
    this->size = size;
    this->usage = usage;
    this->format = format;
}

Texture::~Texture() {
    DestroyTexture();
}

void Texture::CreateTexture() {
    DestroyTexture();
    SDL_GPUTextureCreateInfo createInfo{};
    createInfo.width = (Uint32)size.x;
    createInfo.height = (Uint32)size.y;
    createInfo.layer_count_or_depth = 1;
    createInfo.num_levels = 1;
    createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
    createInfo.format = format;
    createInfo.usage = usage;
    texture = SDL_CreateGPUTexture(device, &createInfo);
    if (!texture) throw std::runtime_error(SDL_GetError());
}

void Texture::DestroyTexture() {
    if (!texture) return;
    SDL_ReleaseGPUTexture(device, texture);
    texture = nullptr;
}

SDL_GPUTexture* Texture::GetGPUTexture() {
    return texture;
}