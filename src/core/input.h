#pragma once

#include "engine.h"

#include "SDL3/SDL_keyboard.h"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_mouse.h"

#include "glm/vec2.hpp"

class Input : public EngineModule {
    public:
        using EngineModule::EngineModule;
        void Init(void) override;
        void Process(void) override;
        void Shutdown(void) override;

        void HandleEvent(const SDL_Event *event);

        void BindInput(const char *name, SDL_Keycode keycode);
        void RebindInput(const char *name, SDL_Keycode keycode);
        void UnbindInput(const char *name);

        
        bool IsHeld(const char *name);
        bool IsPressed(const char *name);
        bool IsReleased(const char *name);

    private:
        const uint8_t Held =     0b00000001;
        const uint8_t Pressed =  0b00000010;
        const uint8_t Released = 0b00000100;
        struct InputBinding {
            char *name;
            SDL_Keycode keycode;
        };

        std::vector<InputBinding> input_bindings;
        std::vector<uint8_t> input_action_states;
        glm::vec2 mouse_rel;

        uint8_t GetState(const char *name);
};