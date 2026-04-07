#include "input.h"
#include "stdio.h"

void Input::Init() {
    BindInput("forward", SDLK_W);
    BindInput("backward", SDLK_S);
    BindInput("left", SDLK_A);
    BindInput("right", SDLK_D);
    BindInput("up", SDLK_SPACE);
    BindInput("down", SDLK_LSHIFT);
    BindInput("speedup", SDLK_EQUALS);
    BindInput("speeddown", SDLK_MINUS);
    BindInput("mouselock", SDLK_L);
    BindInput("break_block", SDLK_E);
}

void Input::Process() {
    for (int i = 0; i < input_bindings.size(); i++) {
        input_action_states[i] &= Held;
    }
    mouse_rel = glm::vec2(0.0f);
}

void Input::Shutdown() {
    for (InputBinding binding : input_bindings) {
        free(binding.name);
    }
}

void Input::HandleEvent(const SDL_Event *event) {
    // handle mouse motion
    if (event->type == SDL_EVENT_MOUSE_MOTION) {
        mouse_rel.x += (float)event->motion.xrel;
        mouse_rel.y += (float)event->motion.yrel;
        return;
    }
    
    // handle keyboard inputs
    bool pressed = event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat;
    bool released = event->type == SDL_EVENT_KEY_UP;

    for (int i = 0; i < input_bindings.size(); i++) {
        if (input_bindings[i].keycode == event->key.key) {
            if (pressed) {
                input_action_states[i] |= Held | Pressed;
            } else if (released) {
                input_action_states[i] |= ~Held;
                input_action_states[i] &= Released;
            }
        }
    }
}

void Input::BindInput(const char *name, SDL_Keycode keycode) {
    InputBinding binding{};
    binding.keycode = keycode;
    binding.name = (char*)malloc(strlen(name)+1);
    strcpy(binding.name, name);
    input_bindings.push_back(binding);
    input_action_states.resize(input_bindings.size());
}

void Input::RebindInput(const char *name, SDL_Keycode keycode) {
    for (size_t i = 0; i < input_bindings.size(); i++) {
        if (strcmp(name, input_bindings[i].name) != 0) continue;
        input_bindings[i].keycode = keycode;
        return;
    }
}

void Input::UnbindInput(const char *name) {
    for (size_t i = 0; i < input_bindings.size(); i++) {
        if (strcmp(name, input_bindings[i].name) != 0) continue;
        free(input_bindings[i].name);

        input_bindings.erase(input_bindings.begin() + i);
        input_action_states.erase(input_action_states.begin() + i);

        return;
    }
}

uint8_t Input::GetState(const char *name) {
    size_t i, size = input_bindings.size();
    for (i = 0; i < size; i++)
        if (strcmp(name, input_bindings[i].name) == 0) break;
    if (i == size) return 0; // if no binding found
    return input_action_states[i];
}

bool Input::IsHeld(const char *name) {
    return GetState(name) & Held;
}

bool Input::IsPressed(const char *name) {
    return GetState(name) & Pressed;
}

bool Input::IsReleased(const char *name) {
    return GetState(name) & Released;
}