#pragma once
#include "engine.h"

#include "SDL3/SDL.h"

class Window : public EngineModule {
    public:
        using EngineModule::EngineModule;
        void Init(void) override;
        void Process(void) override;
        void Shutdown(void) override;

        void SetFullscreen(bool fullscreen);
        bool GetFullscreen(void);
    private:
        SDL_Window* window;
};