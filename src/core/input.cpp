#include "input.h"
#include "window.h"
#include "SDL3/SDL_error.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_log.h"
#include <algorithm>
#include <cmath>

void Input::Init() {
    accepting_input = true;
    if (!event_callback_bound) {
        GetModule<Window>().InputEvent.Bind([this](SDL_Event* event) {
            HandleEvent(event);
        });
        event_callback_bound = true;
    }

    CreateAction("forward");
    CreateAction("backward");
    CreateAction("left");
    CreateAction("right");
    CreateAction("up");
    CreateAction("down");
    CreateAction("lookup");
    CreateAction("lookdown");
    CreateAction("lookleft");
    CreateAction("lookright");
    CreateAction("speedmodifier");

    CreateBinding("forward", SDLK_W);
    CreateBinding("backward", SDLK_S);
    CreateBinding("left", SDLK_A);
    CreateBinding("right", SDLK_D);
    CreateBinding("up", SDLK_SPACE);
    CreateBinding("down", SDLK_LCTRL);
    CreateBinding("speedmodifier", SDLK_LSHIFT);

    CreateGamepadAxisBinding("forward", SDL_GAMEPAD_AXIS_LEFTY, -1);
    CreateGamepadAxisBinding("backward", SDL_GAMEPAD_AXIS_LEFTY, 1);
    CreateGamepadAxisBinding("left", SDL_GAMEPAD_AXIS_LEFTX, -1);
    CreateGamepadAxisBinding("right", SDL_GAMEPAD_AXIS_LEFTX, 1);
    CreateGamepadBinding("up", SDL_GAMEPAD_BUTTON_SOUTH);
    CreateGamepadBinding("down", SDL_GAMEPAD_BUTTON_EAST);
    CreateGamepadBinding("speedmodifier", SDL_GAMEPAD_BUTTON_WEST);

    CreateMouseBinding("lookup", MouseDirection::Up, 0.002f);
    CreateMouseBinding("lookdown", MouseDirection::Down, 0.002f);
    CreateMouseBinding("lookleft", MouseDirection::Left, 0.002f);
    CreateMouseBinding("lookright", MouseDirection::Right, 0.002f);
    CreateGamepadAxisBinding("lookup", SDL_GAMEPAD_AXIS_RIGHTY, -1);
    CreateGamepadAxisBinding("lookdown", SDL_GAMEPAD_AXIS_RIGHTY, 1);
    CreateGamepadAxisBinding("lookleft", SDL_GAMEPAD_AXIS_RIGHTX, -1);
    CreateGamepadAxisBinding("lookright", SDL_GAMEPAD_AXIS_RIGHTX, 1);

    if (!gamepad_subsystem_initialized) {
        gamepad_subsystem_initialized = SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        if (!gamepad_subsystem_initialized) {
            SDL_Log("Gamepad initialization failed: %s", SDL_GetError());
            return;
        }
    }

    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (!ids) {
        SDL_Log("Gamepad enumeration failed: %s", SDL_GetError());
        return;
    }
    for (int i = 0; i < count; ++i) OpenGamepad(ids[i]);
    SDL_free(ids);
}

void Input::Process() {
    for (auto& pair : actions_by_name) {
        pair.second.frame_start_held = (pair.second.action_state & Held) != 0;
        pair.second.action_state &= Held;
    }
    mouse_rel = glm::vec2(0.0f);
    RefreshActions();
}

void Input::Shutdown() {
    accepting_input = false;
    ReleaseAllButtons();
    actions_by_name.clear();
    for (auto& pair : gamepads) SDL_CloseGamepad(pair.second);
    gamepads.clear();
    if (gamepad_subsystem_initialized) {
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        gamepad_subsystem_initialized = false;
    }
}

void Input::HandleMouseEvent(glm::vec2 offset) {
    if (!accepting_input || !std::isfinite(offset.x) || !std::isfinite(offset.y)) return;
    mouse_rel += offset;
    RefreshActions();
}

void Input::HandleKeyEvent(SDL_Keycode key, bool pressed) {
    if (pressed && !accepting_input) return;
    const bool changed = pressed ? keys_down.insert(key).second : keys_down.erase(key) != 0;
    if (changed) RefreshActions();
}

void Input::HandleMouseButtonEvent(Uint8 button, bool pressed) {
    if (pressed && !accepting_input) return;
    const bool changed = pressed ? mouse_buttons_down.insert(button).second : mouse_buttons_down.erase(button) != 0;
    if (changed) RefreshActions();
}

void Input::HandleGamepadButtonEvent(Uint8 button, bool pressed, SDL_JoystickID which) {
    if (pressed && !accepting_input) return;
    bool changed = false;
    if (pressed) {
        changed = gamepad_buttons_down[which].insert(button).second;
    } else {
        auto it = gamepad_buttons_down.find(which);
        if (it != gamepad_buttons_down.end()) {
            changed = it->second.erase(button) != 0;
            if (it->second.empty()) gamepad_buttons_down.erase(it);
        }
    }
    if (changed) RefreshActions();
}

void Input::HandleGamepadAxisEvent(SDL_GamepadAxis axis, Sint16 value, SDL_JoystickID which) {
    if (!accepting_input || axis < 0 || axis >= SDL_GAMEPAD_AXIS_COUNT) return;
    const float normalized = value < 0 ? value / 32768.0f : value / 32767.0f;
    auto& axes = gamepad_axes[which];
    if (axes[axis] == normalized) return;
    axes[axis] = normalized;
    RefreshActions();
}

void Input::HandleEvent(const SDL_Event* event) {
    if (!event) return;
    switch (event->type) {
    case SDL_EVENT_MOUSE_MOTION:
        HandleMouseEvent(glm::vec2(event->motion.xrel, event->motion.yrel));
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        HandleMouseButtonEvent(event->button.button, event->type == SDL_EVENT_MOUSE_BUTTON_DOWN);
        break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        if (!event->key.repeat)
            HandleKeyEvent(event->key.key, event->type == SDL_EVENT_KEY_DOWN);
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        HandleGamepadButtonEvent(event->gbutton.button, event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN, event->gbutton.which);
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        HandleGamepadAxisEvent(static_cast<SDL_GamepadAxis>(event->gaxis.axis), event->gaxis.value, event->gaxis.which);
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
        OpenGamepad(event->gdevice.which);
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        RemoveGamepad(event->gdevice.which);
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        if (event->window.windowID == SDL_GetWindowID(GetModule<Window>().GetWindow())) {
            accepting_input = false;
            ReleaseAllButtons();
        }
        break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        if (event->window.windowID == SDL_GetWindowID(GetModule<Window>().GetWindow()))
            accepting_input = true;
        break;
    }
}

void Input::OpenGamepad(SDL_JoystickID which) {
    if (!gamepad_subsystem_initialized || gamepads.count(which)) return;
    SDL_Gamepad* pad = SDL_OpenGamepad(which);
    if (pad) gamepads.emplace(which, pad);
    else SDL_Log("Could not open gamepad: %s", SDL_GetError());
}

void Input::RemoveGamepad(SDL_JoystickID which) {
    gamepad_buttons_down.erase(which);
    gamepad_axes.erase(which);
    RefreshActions();
    auto it = gamepads.find(which);
    if (it != gamepads.end()) {
        SDL_CloseGamepad(it->second);
        gamepads.erase(it);
    }
}

void Input::ReleaseAllButtons() {
    keys_down.clear();
    mouse_buttons_down.clear();
    gamepad_buttons_down.clear();
    gamepad_axes.clear();
    mouse_rel = glm::vec2(0.0f);
    RefreshActions();
}

void Input::CreateAction(const char* name) {
    if (name) actions_by_name.try_emplace(name);
}

void Input::DeleteAction(const char* name) {
    if (name) actions_by_name.erase(name);
}

void Input::AddBinding(const char* name, Binding binding) {
    if (!name) return;
    InputAction& action = actions_by_name[name];
    auto it = std::find(action.bindings.begin(), action.bindings.end(), binding);
    if (it != action.bindings.end()) *it = binding;
    else action.bindings.push_back(binding);
    RefreshAction(action);
}

void Input::RemoveBinding(const char* name, Binding binding) {
    if (!name) return;
    auto it = actions_by_name.find(name);
    if (it == actions_by_name.end()) return;
    auto& bindings = it->second.bindings;
    bindings.erase(std::remove(bindings.begin(), bindings.end(), binding), bindings.end());
    RefreshAction(it->second);
}

void Input::CreateBinding(const char* name, SDL_Keycode keycode) {
    AddBinding(name, {ButtonType::Keyboard, keycode});
}

void Input::DeleteBinding(const char* name, SDL_Keycode keycode) {
    RemoveBinding(name, {ButtonType::Keyboard, keycode});
}

void Input::CreateMouseBinding(const char* name, Uint8 button) {
    AddBinding(name, {ButtonType::Mouse, button});
}

void Input::DeleteMouseBinding(const char* name, Uint8 button) {
    RemoveBinding(name, {ButtonType::Mouse, button});
}

void Input::CreateMouseBinding(const char* name, MouseDirection direction, float sensitivity) {
    if (!std::isfinite(sensitivity) || sensitivity < 0.0f) return;
    switch (direction) {
    case MouseDirection::Up:
    case MouseDirection::Down:
    case MouseDirection::Left:
    case MouseDirection::Right:
        AddBinding(name, {ButtonType::MouseMotion, static_cast<Uint32>(direction), 1, 0.0f, sensitivity});
        break;
    }
}

void Input::DeleteMouseBinding(const char* name, MouseDirection direction) {
    RemoveBinding(name, {ButtonType::MouseMotion, static_cast<Uint32>(direction)});
}

void Input::CreateGamepadBinding(const char* name, SDL_GamepadButton button) {
    if (button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT) return;
    AddBinding(name, {ButtonType::Gamepad, static_cast<Uint32>(button)});
}

void Input::DeleteGamepadBinding(const char* name, SDL_GamepadButton button) {
    if (button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT) return;
    RemoveBinding(name, {ButtonType::Gamepad, static_cast<Uint32>(button)});
}

void Input::CreateGamepadAxisBinding(const char* name, SDL_GamepadAxis axis, int direction, float deadzone) {
    if (axis < 0 || axis >= SDL_GAMEPAD_AXIS_COUNT || direction == 0 || !std::isfinite(deadzone) || deadzone < 0.0f || deadzone >= 1.0f) return;
    AddBinding(name, {ButtonType::GamepadAxis, static_cast<Uint32>(axis), direction < 0 ? -1 : 1, deadzone});
}

void Input::DeleteGamepadAxisBinding(const char* name, SDL_GamepadAxis axis, int direction) {
    if (axis < 0 || axis >= SDL_GAMEPAD_AXIS_COUNT || direction == 0) return;
    RemoveBinding(name, {ButtonType::GamepadAxis, static_cast<Uint32>(axis), direction < 0 ? -1 : 1});
}

void Input::SetActionThreshold(const char* name, float threshold) {
    if (!name || !std::isfinite(threshold)) return;
    InputAction& action = actions_by_name[name];
    action.threshold = std::clamp(threshold, 0.0f, 1.0f);
    RefreshAction(action);
}

float Input::GetMouseBindingDelta(const Binding& binding) const {
    float motion = 0.0f;
    switch (static_cast<MouseDirection>(binding.code)) {
    case MouseDirection::Up:    motion = -mouse_rel.y; break;
    case MouseDirection::Down:  motion =  mouse_rel.y; break;
    case MouseDirection::Left:  motion = -mouse_rel.x; break;
    case MouseDirection::Right: motion =  mouse_rel.x; break;
    }
    return std::max(motion, 0.0f) * binding.sensitivity;
}

float Input::GetBindingStrength(const Binding& binding) const {
    switch (binding.type) {
    case ButtonType::Keyboard:
        return keys_down.count(binding.code) ? 1.0f : 0.0f;
    case ButtonType::Mouse:
        return mouse_buttons_down.count(static_cast<Uint8>(binding.code)) ? 1.0f : 0.0f;
    case ButtonType::MouseMotion:
        return std::min(GetMouseBindingDelta(binding), 1.0f);
    case ButtonType::Gamepad:
        for (const auto& pair : gamepad_buttons_down)
            if (pair.second.count(static_cast<Uint8>(binding.code))) return 1.0f;
        return 0.0f;
    case ButtonType::GamepadAxis: {
        float strength = 0.0f;
        for (const auto& pair : gamepad_axes) {
            const float directed = pair.second[binding.code] * binding.direction;
            const float value = std::clamp(
                (directed - binding.deadzone) / (1.0f - binding.deadzone), 0.0f, 1.0f);
            strength = std::max(strength, value);
        }
        return strength;
    }
    }
    return 0.0f;
}

void Input::RefreshAction(InputAction& action) {
    const bool was_held = (action.action_state & Held) != 0;
    action.strength = 0.0f;
    for (const Binding& binding : action.bindings) {
        action.strength = std::max(action.strength, GetBindingStrength(binding));
        if (action.strength == 1.0f) break;
    }
    const bool is_held = action.strength > 0.0f && action.strength >= action.threshold;

    const bool has_mouse_motion = std::any_of(action.bindings.begin(), action.bindings.end(),
        [](const Binding& binding) { return binding.type == ButtonType::MouseMotion; });
    if (has_mouse_motion) {
        action.action_state = is_held ? Held : 0;
        if (is_held && !action.frame_start_held) action.action_state |= Pressed;
        if (!is_held && action.frame_start_held) action.action_state |= Released;
        return;
    }

    if (is_held && !was_held) action.action_state |= Held | Pressed;
    else if (!is_held && was_held) {
        action.action_state &= static_cast<uint8_t>(~Held);
        action.action_state |= Released;
    }
}

void Input::RefreshActions() {
    for (auto& pair : actions_by_name) RefreshAction(pair.second);
}

uint8_t Input::GetState(const char* name) {
    if (!name) return 0;
    auto it = actions_by_name.find(name);
    return it == actions_by_name.end() ? 0 : it->second.action_state;
}

float Input::GetStrength(const char* name) {
    if (!name) return 0.0f;
    auto it = actions_by_name.find(name);
    return it == actions_by_name.end() ? 0.0f : it->second.strength;
}

float Input::GetDelta(const char* name, float deltaTime) {
    if (!name || !std::isfinite(deltaTime) || deltaTime < 0.0f) return 0.0f;
    auto it = actions_by_name.find(name);
    if (it == actions_by_name.end()) return 0.0f;
    float delta = 0.0f;
    for (const Binding& binding : it->second.bindings) {
        const float contribution = binding.type == ButtonType::MouseMotion
            ? GetMouseBindingDelta(binding) : GetBindingStrength(binding) * deltaTime;
        delta = std::max(delta, contribution);
    }
    return delta;
}
bool Input::IsHeld(const char* name) { return (GetState(name) & Held) != 0; }
bool Input::IsPressed(const char* name) { return (GetState(name) & Pressed) != 0; }
bool Input::IsReleased(const char* name) { return (GetState(name) & Released) != 0; }
glm::vec2 Input::GetMouseMovement() { return mouse_rel; }

void Input::SetMouseLock(bool lock) {
    Window& window = GetModule<Window>();
    SDL_SetWindowMouseGrab(window.GetWindow(), lock);
    SDL_SetWindowRelativeMouseMode(window.GetWindow(), lock);
}

bool Input::GetMouseLock() {
    return SDL_GetWindowRelativeMouseMode(GetModule<Window>().GetWindow());
}