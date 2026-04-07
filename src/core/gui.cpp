#include "gui.h"

#include "window.h"
#include "modules/renderer/renderer.h"

void GUI::Init() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags = ImGuiConfigFlags_None;

    ImGui::StyleColorsDark();

    // Platform backend
    ImGui_ImplSDL3_InitForSDLGPU(GetModule<Window>().GetWindow());

    // Renderer backend
    ImGui_ImplSDLGPU3_InitInfo init{};
    init.Device = GetModule<Renderer>().GetDevice();
    init.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(GetModule<Renderer>().GetDevice(), GetModule<Window>().GetWindow());
    init.MSAASamples = SDL_GPU_SAMPLECOUNT_1;

    ImGui_ImplSDLGPU3_Init(&init);

    GetModule<Window>().InputEvent.Bind(
        [this](SDL_Event* event) {
            ProcessEvent(event);
        }
    );
}

void GUI::PreProcess() {
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void GUI::Shutdown() {
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void GUI::ProcessEvent(SDL_Event *event) {
    ImGui_ImplSDL3_ProcessEvent(event);
}

void GUI::Render(SDL_GPUCommandBuffer* cmd, SDL_GPURenderPass* pass) {
    ImGui_ImplSDLGPU3_RenderDrawData(
        ImGui::GetDrawData(),
        cmd,
        pass,
        nullptr
    );
}