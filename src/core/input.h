#pragma once

#include "engine.h"

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_gamepad.h"
#include "SDL3/SDL_keyboard.h"
#include "SDL3/SDL_mouse.h"

#include "glm/vec2.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Input : public EngineModule {
public:
    using EngineModule::EngineModule;

    enum class MouseDirection {
        Up,
        Down,
        Left,
        Right
    };

    void Init() override;
    void Process() override;
    void Shutdown() override;

    void HandleEvent(const SDL_Event* event);
    void HandleMouseEvent(glm::vec2 offset);
    void HandleKeyEvent(SDL_Keycode key, bool pressed);
    void HandleMouseButtonEvent(Uint8 button, bool pressed);

    void HandleGamepadButtonEvent(Uint8 button, bool pressed, SDL_JoystickID which = 0);
    void HandleGamepadAxisEvent(SDL_GamepadAxis axis, Sint16 value, SDL_JoystickID which = 0);

    void CreateAction(const char* name);
    void DeleteAction(const char* name);

    void CreateBinding(const char* name, SDL_Keycode keycode);
    void DeleteBinding(const char* name, SDL_Keycode keycode);

    void CreateMouseBinding(const char* name, Uint8 button);
    void DeleteMouseBinding(const char* name, Uint8 button);

    void CreateMouseBinding(const char* name, MouseDirection direction, float sensitivity);
    void DeleteMouseBinding(const char* name, MouseDirection direction);

    void CreateGamepadBinding(const char* name, SDL_GamepadButton button);
    void DeleteGamepadBinding(const char* name, SDL_GamepadButton button);


    void CreateGamepadAxisBinding(const char* name, SDL_GamepadAxis axis, int direction = 1, float deadzone = 0.15f);
    void DeleteGamepadAxisBinding(const char* name, SDL_GamepadAxis axis, int direction = 1);

    void SetActionThreshold(const char* name, float threshold);

    uint8_t GetState(const char* name);
    float GetStrength(const char* name);
    float GetDelta(const char* name, float deltaTime);

    bool IsHeld(const char* name);
    bool IsPressed(const char* name);
    bool IsReleased(const char* name);

    glm::vec2 GetMouseMovement();

    void SetMouseLock(bool lock);
    bool GetMouseLock();

private:
    static constexpr uint8_t Held     = 0b00000001;
    static constexpr uint8_t Pressed  = 0b00000010;
    static constexpr uint8_t Released = 0b00000100;

    enum class ButtonType {
        Keyboard,
        Mouse,
        Gamepad,
        GamepadAxis,
        MouseMotion
    };

    struct Binding {
        ButtonType type;
        Uint32 code;
        int direction = 1;
        float deadzone = 0.15f;
        float sensitivity = 1.0f;

        bool operator==(const Binding& other) const {
            return type == other.type
                && code == other.code
                && direction == other.direction;
        }
    };

    struct InputAction {
        uint8_t action_state = 0;
        float strength = 0.0f;
        float threshold = 0.5f;
        bool frame_start_held = false;
        std::vector<Binding> bindings;
    };

    void AddBinding(const char* name, Binding binding);
    void RemoveBinding(const char* name, Binding binding);

    float GetBindingStrength(const Binding& binding) const;
    float GetMouseBindingDelta(const Binding& binding) const;

    void RefreshAction(InputAction& action);
    void RefreshActions();
    void ReleaseAllButtons();

    void OpenGamepad(SDL_JoystickID which);
    void RemoveGamepad(SDL_JoystickID which);

    std::unordered_map<std::string, InputAction> actions_by_name;
    std::unordered_set<SDL_Keycode> keys_down;
    std::unordered_set<Uint8> mouse_buttons_down;
    std::unordered_map<SDL_JoystickID, std::unordered_set<Uint8>> gamepad_buttons_down;
    std::unordered_map<SDL_JoystickID, std::array<float, SDL_GAMEPAD_AXIS_COUNT>> gamepad_axes;
    std::unordered_map<SDL_JoystickID, SDL_Gamepad*> gamepads;
    glm::vec2 mouse_rel = glm::vec2(0.0f);

    bool gamepad_subsystem_initialized = false;
    bool accepting_input = true;
    bool event_callback_bound = false;
};