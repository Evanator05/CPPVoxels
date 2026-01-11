#include "i_input.h"

SDL_Keycode input_bindings[BINDING_COUNT] = { SDLK_W, SDLK_S, SDLK_A, SDLK_D, SDLK_SPACE, SDLK_LSHIFT, SDLK_EQUALS, SDLK_MINUS, SDLK_L };
ACTIONSTATE input_action_states[BINDING_COUNT] = {};

glm::vec2 mouse_rel = {};

void input_beginframe() {
    for (int i = 0; i < BINDING_COUNT; i++) {
        input_action_states[i] &= ACTIONSTATE_HELD;
    }
    mouse_rel = glm::vec2(0.0f);
}

void input_handleevent(const SDL_Event *e) {
    // handle mouse motion
    if (e->type == SDL_EVENT_MOUSE_MOTION) {
        mouse_rel.x += (float)e->motion.xrel;
        mouse_rel.y += (float)e->motion.yrel;
        return;
    }
    
    // handle keyboard inputs
    bool pressed = e->type == SDL_EVENT_KEY_DOWN && !e->key.repeat;
    bool released = e->type == SDL_EVENT_KEY_UP;

    for (int i = 0; i < BINDING_COUNT; i++) {
        if (input_bindings[i] == e->key.key) {
            if (pressed) {
                input_action_states[i] |= ACTIONSTATE_HELD | ACTIONSTATE_PRESSED;
            } else if (released) {
                input_action_states[i] |= ~ACTIONSTATE_HELD;
                input_action_states[i] &= ACTIONSTATE_RELEASED;
            }
        }
    }
}

void input_endframe() {

}

ACTIONSTATE input_getstate(INPUT_ACTIONS action) {
    return input_action_states[action];
}

bool input_isheld(INPUT_ACTIONS action) {
    return input_getstate(action) & ACTIONSTATE_HELD;
}

bool input_ispressed(INPUT_ACTIONS action) {
    return input_getstate(action) & ACTIONSTATE_PRESSED;
}

bool input_isreleased(INPUT_ACTIONS action) {
    return input_getstate(action) & ACTIONSTATE_RELEASED;
}

glm::vec2 input_getmouse_rel() {
    return mouse_rel;
}

void input_set_mouse_lock(bool lock) {
    SDL_SetWindowMouseGrab(video_get_window(), lock);
    SDL_SetWindowRelativeMouseMode(video_get_window(), lock);
}

bool input_get_mouse_lock() {
    return SDL_GetWindowMouseGrab(video_get_window());
}