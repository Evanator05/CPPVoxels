#pragma once
#include "engine.h"

#include "glm/vec2.hpp"
#include "SDL3/SDL.h"

class Window : public EngineModule {
    public:
        using EngineModule::EngineModule;
        void Init(void) override;
        void Process(void) override;
        void Shutdown(void) override;

        void SetFullscreen(bool fullscreen);
        bool GetFullscreen(void);

        glm::ivec2 GetSize();
        
        SDL_Window* GetWindow();
    private:
        SDL_Window* window;
};