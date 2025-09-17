#include "stdio.h"

#include "e_engine.h"
#include "i_video.h"
#include "i_graphics.h"
#include "voxel.h"
#include "i_gpubuffers.h"
#include "worldinfo.h"

#include "time.h"
#include "stdlib.h"

#include "math.h"

bool running = true;

int toColor(int x) {
    return x*32/64;
}

void engine_init() {
    video_init();
    graphics_init();
    voxel_init();
    //gpubuffers_init();
    
    for (int x = 0; x < 4; x++) {
        for (int z = 0; z < 4; z++) {
            for (int y = 0; y < 4; y++) {
                voxel_chunkAllocate(glm::ivec3(x, y, z));
            }
        }
    }

    // srand(time(NULL));
    // for (int i = 0; i < chunkData.size(); i++) {
    //     Chunk chunk = chunkData[i];
    //     chunk.data.index;
    //     for (int j = 0; j < chunk.data.size.x*chunk.data.size.y*chunk.data.size.z; j++) {
    //         int number = j;
    //         int z = number/(64*64);
    //         number %= 64*64;
    //         int y = number/64;
    //         int x = number%64;

    //         voxelData[chunk.data.index+j].data = (VOXELSOLID*(rand()&1)) | rand(); //(VOXELSOLID*(rand()&1)) | (toColor(z)<<10) | (toColor(y)<<5) | toColor(x);
    //     }
    // }

    gpubuffers_init();
    gpubuffers_upload();

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

void engine_update() {

    worldInfo.cameraPos = glm::vec3(
        sin(worldInfo.time/20)*64+128,
        150,
        cos(worldInfo.time/20)*64+128
    );
    worldInfo.cameraRot = glm::vec2(worldInfo.time/20+3.14,.45);

    for (int i = 0; i < 128; i++) {
        voxelData[rand32()%voxelData.size()].data = (VOXELSOLID*(rand()&1)) | rand();
    }
    
    gpubuffers_upload();

    worldInfo_update();
    video_update();
    graphics_update();
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