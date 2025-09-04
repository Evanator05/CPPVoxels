/*
    Handles the creation of the window using sdl3
*/

#include "SDL3/SDL.h"

extern SDL_Window* window;

// initializes the window
void video_init(void);

// cleans up the window
void video_cleanup(void);

// copies a frame buffer to the window
void video_update(void);

// gets the initialized sdl window
SDL_Window* video_get_window(void);