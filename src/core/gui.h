#pragma once

#include "engine.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlgpu3.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

class GUI : public EngineModule {
    public:
        using EngineModule::EngineModule;
        void Init(void) override;
        void PreProcess(void) override;
        void PostProcess(void) override;
        void Shutdown(void) override;

        void ProcessEvent(SDL_Event *event);
        void Render(SDL_GPUCommandBuffer* cmd, SDL_GPURenderPass* pass);
};