#include "SDL3/SDL.h"
#include "SDL3/SDL_gpu.h"

// initializes the graphics pipeline
void graphics_init(void);

// cleans up the graphics pipeline
void graphics_cleanup(void);

// renders a frame
void graphics_update(void);

SDL_GPUDevice* graphics_getDevice(void);

// initialize the sdl gpu device
void initDevice(void);

// create the display texture
void initDisplayTextures(void);

// create the compute pipeline
void initComputePipeline(void);

// draws a frame
void drawFrame(void);

// resizes the display textures
void graphics_resize(void);
