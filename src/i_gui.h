#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlgpu3.h" // Or equivalent SDLGPU backend
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

void gui_init(void);
void gui_process_event(SDL_Event *event);
void gui_begin_frame(void);
void gui_render(SDL_GPUCommandBuffer* cmd, SDL_GPURenderPass* pass);
void gui_cleanup(void);
