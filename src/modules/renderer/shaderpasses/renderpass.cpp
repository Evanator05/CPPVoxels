#include "renderpass.h"

void RenderPass::Create() {

}
void RenderPass::Destroy() {

}
void RenderPass::Execute(SDL_GPUCommandBuffer* cmd) {
    SDL_GPUColorTargetInfo colorTarget{};
    colorTarget.texture = output->GetGPU();
    colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
    colorTarget.store_op = SDL_GPU_STOREOP_STORE;
    colorTarget.clear_color = {0.2f, 0.2f, 0.4f, 1.0f};

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &colorTarget, 1, nullptr); // need to fix color targerts

    SDL_BindGPUGraphicsPipeline(pass, nullptr); // need to create pipeline

    SDL_GPUBufferBinding binding{};
    binding.buffer = vertices->GetGPU();
    binding.offset = 0;
    SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);

    SDL_DrawGPUPrimitives(pass, 0, 0, 0, 0);

    SDL_EndGPURenderPass(pass);
}