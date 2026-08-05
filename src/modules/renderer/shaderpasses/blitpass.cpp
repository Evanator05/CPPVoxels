#include "blitpass.h"

void BlitPass::Create(void) {

}
void BlitPass::Destroy(void) {

};
void BlitPass::Execute(SDL_GPUCommandBuffer* cmd) {
    SDL_GPUBlitInfo blt{};
    blt.source.texture = source->GetGPU();
    blt.source.w = source->size.x;
    blt.source.h = source->size.y;
    blt.destination.texture = destination->GetGPU();
    blt.destination.w = destination->size.x;
    blt.destination.h = destination->size.y;
    SDL_BlitGPUTexture(cmd, &blt);
};