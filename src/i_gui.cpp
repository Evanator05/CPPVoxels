#include "i_gui.h"
#include "i_video.h"
#include "i_graphics.h"

void gui_init() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags = ImGuiConfigFlags_None;

    ImGui::StyleColorsDark();

    // Platform backend (window + input)
    ImGui_ImplSDL3_InitForSDLGPU(video_get_window());

    // Renderer backend (THIS WAS MISSING)
    ImGui_ImplSDLGPU3_InitInfo init{};
    init.Device = graphics_getDevice();
    init.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(graphics_getDevice(), video_get_window()); // or your swapchain format
    init.MSAASamples = SDL_GPU_SAMPLECOUNT_1;

    ImGui_ImplSDLGPU3_Init(&init);
}

void gui_process_event(SDL_Event *event) {
    ImGui_ImplSDL3_ProcessEvent(event);
}

void gui_begin_frame() {
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void gui_render(SDL_GPUCommandBuffer* cmd, SDL_GPURenderPass* pass)
{
    
    
    ImGui_ImplSDLGPU3_RenderDrawData(
        ImGui::GetDrawData(),
        cmd,
        pass,
        nullptr   // let ImGui use its internal pipeline
    );
}

void gui_cleanup() {
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}
