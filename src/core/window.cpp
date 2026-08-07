#include "window.h"


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
    icon = SDL_CreateSurface(8, 8, SDL_PIXELFORMAT_XRGB8888);
    
    if (icon)
    {
        SDL_LockSurface(icon);

        uint32_t* pixels = static_cast<uint32_t*>(icon->pixels);

        for (int y = 0; y < icon->h; ++y)
        {
            uint32_t* row = reinterpret_cast<uint32_t*>(
                static_cast<uint8_t*>(icon->pixels) + y * icon->pitch);

            for (int x = 0; x < icon->w; ++x)
            {
                uint8_t r = rand() % 256;
                uint8_t g = rand() % 256;
                uint8_t b = rand() % 256;

                row[x] = (r << 16) | (g << 8) | b;
            }
        }

        SDL_UnlockSurface(icon);
        SDL_SetWindowIcon(window, icon);
    }
    
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return;
    }
}

void Window::Process() {
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                engine->Quit();
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                ResizedScreen.Emit(GetSize());
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE)
                    engine->Quit();
                if (event.key.key == SDLK_F11) {
                    SetFullscreen(!GetFullscreen());
                    ResizedScreen.Emit(GetSize());
                }
                break;
        }
        InputEvent.Emit(&event);
    }
}

void Window::Shutdown() {
    SDL_DestroySurface(icon);
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