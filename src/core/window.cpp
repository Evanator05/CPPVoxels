#include "window.h"
#include "input.h"
#include "modules/renderer/renderer.h"

void Window::Init() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return;
    }
    
    // SDL_DisplayID display = SDL_GetPrimaryDisplay();

    // const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(display);
    // if (!mode) {
    //     SDL_Log("Failed to get display mode: %s", SDL_GetError());
    // }

    window = SDL_CreateWindow("Voxels", 720, 480, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return;
    }
}

void Window::Process() {
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        //gui_process_event(&event);
        switch (event.type) {
            case SDL_EVENT_QUIT:
                engine->Quit();
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                GetModule<Renderer>().UpdateDisplayTextures();
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE)
                    engine->Quit();
                if (event.key.key == SDLK_F11) {
                    SetFullscreen(!GetFullscreen());
                    GetModule<Renderer>().UpdateDisplayTextures();
                }
                break;
        }
        GetModule<Input>().HandleEvent(&event);
    }
}

void Window::Shutdown() {
    SDL_DestroyWindow(window);
    window = NULL;
    SDL_Quit();
}

void Window::SetFullscreen(bool fullscreen) {
    SDL_SetWindowFullscreen(window, fullscreen);   
}

bool Window::GetFullscreen() {
    return SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN;
}

glm::ivec2 Window::GetSize() {
    glm::ivec2 size;
    SDL_GetWindowSizeInPixels(window, &size.x, &size.y);
    return size;
}

SDL_Window* Window::GetWindow() {
    return window;
}