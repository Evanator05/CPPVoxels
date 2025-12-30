#include "i_video.h"
#include "e_engine.h"
#include "i_input.h"

SDL_Window *window = NULL;

void video_init(void) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return;
    }
    
    window = SDL_CreateWindow("Voxels", 2560, 1440, SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return;
    }
}

void video_cleanup(void) {
    SDL_DestroyWindow(window);
    window = NULL;
    SDL_Quit();
}

void video_update(void) {
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                engine_quit();
                break;
            default:
                input_handleevent(&event);
                break;
        }
    }
}

SDL_Window* video_get_window(void) {
    return window;
}