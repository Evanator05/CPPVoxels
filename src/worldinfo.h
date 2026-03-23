#include "SDL3/SDL_gpu.h"

#include "glm/vec3.hpp"
#include "glm/vec2.hpp"
#include "glm/mat4x4.hpp"
#include "camera.h"

typedef struct _WorldInfo {
    glm::mat4 cameraTransform;
    float time;
} WorldInfo;

extern WorldInfo worldInfo;
extern SDL_GPUBuffer *worldInfoBuffer;
extern SDL_GPUTransferBuffer *worldInfoTransferBuffer;
extern Camera camera;

void worldInfo_CreateBuffers(void);
void worldInfo_TransforToGPU(void);

void worldInfo_init(void);
void worldInfo_update(void);
void worldInfo_cleanup(void);