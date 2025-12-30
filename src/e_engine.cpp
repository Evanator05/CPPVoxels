#include "stdio.h"

#include "e_engine.h"
#include "i_video.h"
#include "i_input.h"
#include "i_graphics.h"
#include "voxel.h"
#include "i_gpubuffers.h"
#include "worldinfo.h"

#include "chunkbvh.h"

#include "time.h"
#include "stdlib.h"

#include "math.h"

bool running = true;

int toColor(int x) {
    return x%32;
}

static inline uint32_t hash3(int x, int y, int z) {
    return (uint32_t)(x * 73856093 ^ y * 19349663 ^ z * 83492791);
}

static inline uint32_t hash1(uint32_t x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

void gen_test_chunks(void) {
    for (int x = 0; x < 4; x++) {
        for (int z = 0; z < 4; z++) {
            for (int y = 0; y < 4; y++) {
                voxel_chunkAllocate(glm::ivec3(x, y, z));
            }
        }
    }
    
    srand(time(NULL));
    for (int i = 0; i < chunkData.size(); i++) {
        Chunk chunk = chunkData[i];
        int offset = chunk.data.index;
        for (int j = 0; j < chunk.data.size.x*chunk.data.size.y*chunk.data.size.z; j++) {
            int number = j;
            int z = number/(64*64);
            number %= 64*64;
            int y = number/64;
            int x = number%64;

            int gx = x + chunk.pos.x*64;
            int gy = y + chunk.pos.y*64;
            int gz = z + chunk.pos.z*64;

            float h1 = sin(gx * 0.1f) * 10.0f;
            float h2 = cos(gz * 0.2f) * 5.0f;
            float h3 = sin((gx + gz) * 0.01f) * 8.0f;
            int terrainHeight = (int)(120 + h1 + h2 + h3);

            if (gx%16 < 2 || gz%16 < 2 || gy%16 < 2) {
                voxelData[offset+j].data = (rand()) | VOXELSOLID*(gy<terrainHeight || gy>(terrainHeight+64));
            } else {
                number = rand()%32;
                voxelData[offset+j].data = (number | number<<5 | number<<10) | VOXELSOLID*(gy<terrainHeight || gy>(terrainHeight+64));
            }
            
        }
    }
}

void engine_init() {
    video_init();
    graphics_init();
    voxel_init();
    
    gen_test_chunks();

    gpubuffers_init();
    gpubuffers_upload();
    chunkbvh_buildFromChunks(chunkData);
    worldInfo_init();
}

void engine_cleanup() {
    worldInfo_cleanup();
    gpubuffers_cleanup();
    graphics_cleanup();
    video_cleanup();
}

uint32_t rand32() {
    return ((uint32_t)rand() << 30) ^ ((uint32_t)rand() << 15) ^ (uint32_t)rand();
}

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
void move_camera() {
    constexpr float MOVESPEED = 0.2f;
    constexpr float CAMSPEED  = 0.02f;

    // Camera rotation

    if (input_isheld(LOOK_UP))    worldInfo.cameraRot.y -= CAMSPEED;
    if (input_isheld(LOOK_DOWN))  worldInfo.cameraRot.y += CAMSPEED;
    if (input_isheld(LOOK_LEFT))  worldInfo.cameraRot.x -= CAMSPEED;
    if (input_isheld(LOOK_RIGHT)) worldInfo.cameraRot.x += CAMSPEED;

    // Clamp pitch to avoid flipping
    constexpr float MAX_PITCH = glm::radians(89.0f);
    worldInfo.cameraRot.y = glm::clamp(
        worldInfo.cameraRot.y,
        -MAX_PITCH,
        MAX_PITCH
    );

    //Direction vectors

    const float yaw   = worldInfo.cameraRot.x;
    const float pitch = -worldInfo.cameraRot.y;

    // Forward vector
    glm::vec3 forward = glm::normalize(glm::vec3(
        cosf(pitch) * sinf(yaw),
        sinf(pitch),
        cosf(pitch) * cosf(yaw)
    ));

    // Right vector
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

    // Up vector
    glm::vec3 up = glm::normalize(glm::cross(right, forward));

    // Movement input

    glm::vec3 move(0.0f);

    if (input_isheld(FORWARD))  move += forward;
    if (input_isheld(BACKWARD)) move -= forward;
    if (input_isheld(RIGHT))    move -= right;
    if (input_isheld(LEFT))     move += right;
    if (input_isheld(UP))       move += up;
    if (input_isheld(DOWN))     move -= up;

    // Prevent diagonal speed boost
    if (glm::dot(move, move) > 0.0f) {
        move = glm::normalize(move);
    }

    // Apply movement
    worldInfo.cameraPos += move * MOVESPEED;
}

void engine_update() {
    input_beginframe();
    video_update();

    // handle game logic here
    move_camera();

    // start rendering
    worldInfo_update();
    graphics_update();
    input_endframe();
}

void engine_loop() {
    while(running) {
        engine_update();
    }
}

int engine_main() {
    engine_init();
    engine_loop();
    engine_cleanup();

    return 0;
}

void engine_quit() {
    running = false;
}