#include "imguipass.h"

void ImGuiPass::Create(void) {

}
void ImGuiPass::Destroy(void) {

};
void ImGuiPass::Execute(SDL_GPUCommandBuffer* cmd) {
    SDL_GPUColorTargetInfo color{};
    color.texture = destination->GetGPU();
    color.load_op  = SDL_GPU_LOADOP_LOAD;
    color.store_op = SDL_GPU_STOREOP_STORE;

    ImGui::Render();
    ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), cmd);
    SDL_GPURenderPass *rpass = SDL_BeginGPURenderPass(cmd, &color, 1, nullptr);
    ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), cmd, rpass, nullptr);
    SDL_EndGPURenderPass(rpass);
};