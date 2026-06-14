#pragma once

#include "engine.h"

#include "SDL3/SDL_keyboard.h"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_mouse.h"

#include "glm/vec2.hpp"

#include <unordered_map>
#include <unordered_map>

class Input : public EngineModule {
    public:
        using EngineModule::EngineModule;
        void Init(void) override;
        void Process(void) override;
        void Shutdown(void) override;

        void HandleEvent(const SDL_Event *event);
        void HandleMouseEvent(glm::vec2 offset);
        void HandleKeyEvent(SDL_Keycode key, bool pressed);
        void HandleGamepadButtonEvent(Uint8 button, bool pressed);


        void CreateAction(const char *name);
        void DeleteAction(const char *name);

        void CreateBinding(const char *name, SDL_Keycode keycode);
        void DeleteBinding(const char *name, SDL_Keycode keycode);

        uint8_t GetState(const char *name);
        uint8_t GetStrength(const char *name);
        bool IsHeld(const char *name);
        bool IsPressed(const char *name);
        bool IsReleased(const char *name);
        glm::vec2 GetMouseMovement(void);

    private:
        const uint8_t Held =     0b00000001;
        const uint8_t Pressed =  0b00000010;
        const uint8_t Released = 0b00000100;
        struct InputAction {
            char *name;
            uint8_t action_state;
        };
        std::unordered_map<std::string, InputAction> actions_by_name;
        std::unordered_multimap<SDL_Keycode, InputAction*> actions_by_keycode;

        glm::vec2 mouse_rel; 
};