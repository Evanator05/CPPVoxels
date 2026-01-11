#include "i_video.h"
#include "e_engine.h"
#include "i_input.h"
#include "i_gui.h"

SDL_Window *window = NULL;

void video_init(void) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return;
    }
    
    // SDL_DisplayID display = SDL_GetPrimaryDisplay();

    // const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(display);
    // if (!mode) {
    //     SDL_Log("Failed to get display mode: %s", SDL_GetError());
    // }

    window = SDL_CreateWindow("Voxels", 0, 0, SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN);
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
        gui_process_event(&event);
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