#pragma once

#include <stdint.h>

#include "SDL3/SDL_keyboard.h"
#include "SDL3/SDL_events.h"

// just a number, for pressure sensitive stuff like triggers or thumbsticks the number is how hard its pressed
typedef uint8_t ACTIONSTATE;
#define ACTIONSTATE_HELD     0b00000001
#define ACTIONSTATE_PRESSED  0b00000010
#define ACTIONSTATE_RELEASED 0b00000100
#define ACTIONSTATE_DEFAULT 0

typedef enum _INPUT_ACTIONS {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN,
    LOOK_LEFT,
    LOOK_RIGHT,
    LOOK_UP,
    LOOK_DOWN,
    SPEEDUP,
    SPEEDDOWN
} INPUT_ACTIONS;

#define BINDING_COUNT 12

void input_beginframe(void);
void input_handleevent(const SDL_Event *e);
void input_endframe(void);

ACTIONSTATE input_getstate(INPUT_ACTIONS action);
bool input_isheld(INPUT_ACTIONS action);
bool input_ispressed(INPUT_ACTIONS action);
bool input_isreleased(INPUT_ACTIONS action);