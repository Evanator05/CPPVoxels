#include "SDL3/SDL_gpu.h"

#include "glm/vec3.hpp"
#include "glm/vec2.hpp"

typedef struct _WorldInfo {
    glm::vec3 cameraPos;
    float time;
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