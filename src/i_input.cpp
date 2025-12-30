#include "i_input.h"

SDL_Keycode input_bindings[BINDING_COUNT] = { SDLK_W, SDLK_S, SDLK_A, SDLK_D, SDLK_SPACE, SDLK_LSHIFT, SDLK_J, SDLK_L, SDLK_I, SDLK_K };
ACTIONSTATE input_action_states[BINDING_COUNT] = {};

void input_beginframe() {
    for (int i = 0; i < BINDING_COUNT; i++) {
        input_action_states[i] &= ACTIONSTATE_HELD;
    }
}

void input_handleevent(const SDL_Event *e) {
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