#include "input.h"
#include "stdio.h"

#include "window.h"

void Input::Init() {
    GetModule<Window>().InputEvent.Bind(
        [this](SDL_Event* event) {
            HandleEvent(event);
        }
    );

    CreateAction("forward");
    CreateAction("backward");
    CreateAction("left");
    CreateAction("right");
    CreateAction("up");
    CreateAction("down");
    CreateAction("speedup");
    CreateAction("speeddown");
    CreateAction("mouselock");
    CreateAction("break_block");
    
    CreateBinding("forward", SDLK_W);
    CreateBinding("backward", SDLK_S);
    CreateBinding("left", SDLK_A);
    CreateBinding("right", SDLK_D);
    CreateBinding("up", SDLK_SPACE);
    CreateBinding("down", SDLK_LSHIFT);
    CreateBinding("speedup", SDLK_EQUALS);
    CreateBinding("speeddown", SDLK_MINUS);
    CreateBinding("mouselock", SDLK_L);
    CreateBinding("break_block", SDLK_E);
}

void Input::Process() {
    for (auto& action : actions_by_name) {
        action.second.action_state &= Held;
    }
    mouse_rel = glm::vec2(0.0f);
}

void Input::Shutdown() {
    for (auto action : actions_by_name) {
        free(action.second.name);
    }
}

void Input::HandleMouseEvent(glm::vec2 offset) {
    mouse_rel += offset;
}

void Input::HandleKeyEvent(SDL_Keycode key, bool pressed) {
    auto range = actions_by_keycode.equal_range(key);

    for (auto it = range.first; it != range.second; ++it) {
        InputAction* action = it->second;
        if (pressed) {
            action->action_state |= (Held | Pressed);
        } else {
            action->action_state &= ~Held;
            action->action_state |= Released;
        }
    }
}

void Input::HandleGamepadButtonEvent(Uint8 button, bool pressed) {
    
}

void Input::HandleEvent(const SDL_Event *event) {
    switch (event->type) {
        case SDL_EVENT_MOUSE_MOTION:
            HandleMouseEvent(glm::vec2(event->motion.xrel, event->motion.yrel));
            break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            if (event->key.repeat) break; // dont handle repeats
            HandleKeyEvent(event->key.key, event->type == SDL_EVENT_KEY_DOWN);
            break;

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            HandleGamepadButtonEvent(event->gbutton.button, event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
            break;
    }
}

void Input::CreateAction(const char *name) {
    InputAction action{};
    action.name = (char*)malloc(strlen(name)+1);
    strcpy(action.name, name);
    actions_by_name[name] = action;
}

void Input::DeleteAction(const char *name) {
    auto it = actions_by_name.find(name);
    if (it == actions_by_name.end())
        return;

    InputAction* action = &it->second;

    // remove all bindings pointing to this action
    for (auto bindIt = actions_by_keycode.begin(); bindIt != actions_by_keycode.end();) {
        if (bindIt->second == action)
            bindIt = actions_by_keycode.erase(bindIt);
        else
            ++bindIt;
    }
    // remove action
    free(actions_by_name[name].name);
    actions_by_name.erase(it);
}

void Input::CreateBinding(const char *name, SDL_Keycode keycode) {
    actions_by_keycode.emplace(keycode, &actions_by_name[name]);
}

void Input::DeleteBinding(const char *name, SDL_Keycode keycode) {
    auto range = actions_by_keycode.equal_range(keycode);
    for (auto it = range.first; it != range.second; ) {
        if (it->second && it->second->name && strcmp(it->second->name, name) == 0) {
            it = actions_by_keycode.erase(it);
        } else {
            ++it;
        }
    }
}

uint8_t Input::GetState(const char *name) {
    if (!actions_by_name.contains(name)) return 0;
    return actions_by_name[name].action_state;
}

uint8_t Input::GetStrength(const char *name) {
    return GetState(name);
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

glm::vec2 Input::GetMouseMovement() {
    return mouse_rel;
}