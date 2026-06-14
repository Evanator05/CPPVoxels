#include "shaderpass.h"

ShaderPass::ShaderPass(SDL_GPUDevice *device) {
    this->device = device;
}

ShaderPass::~ShaderPass() {
    Destroy();
}

void ShaderPass::Destroy() {
    
}