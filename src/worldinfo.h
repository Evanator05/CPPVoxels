#include "SDL3/SDL_gpu.h"

#include "glm/vec3.hpp"
#include "glm/vec2.hpp"

typedef struct _WorldInfo {
    float time;
    glm::vec3 cameraPos;
    glm::vec2 cameraRot; 
} WorldInfo;

extern WorldInfo worldInfo;
extern SDL_GPUBuffer *worldInfoBuffer;
extern SDL_GPUTransferBuffer *worldInfoTransferBuffer;

void worldInfo_CreateBuffers(void);
void worldInfo_TransforToGPU(void);

void worldInfo_init(void);
void worldInfo_update(void);
void worldInfo_cleanup(void);