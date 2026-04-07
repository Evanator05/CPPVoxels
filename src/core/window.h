#pragma once
#include "engine.h"
#include "event.h"

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

        Event<glm::ivec2> ResizedScreen;
        Event<SDL_Event*> InputEvent;
    private:
        SDL_Window* window;
};