#include "stdio.h"

#include "e_engine.h"
#include "i_video.h"
#include "i_graphics.h"
#include "voxel.h"
#include "i_gpubuffers.h"

bool running = true;

int toColor(int x) {
    return x*32/64;
}

void engine_init() {
    video_init();
    graphics_init();
    voxel_init();
    //gpubuffers_init();
    
    //allocate some functions to test, delete in final project
    voxel_chunkAllocate(glm::ivec3(0, 0, 0));
    voxel_chunkAllocate(glm::ivec3(0, 1, 0));
    voxel_chunkAllocate(glm::ivec3(1, 0, 0));
    voxel_chunkAllocate(glm::ivec3(1, 1, 0));
    voxel_chunkAllocate(glm::ivec3(0, 0, 1));
    voxel_chunkAllocate(glm::ivec3(0, 1, 1));
    voxel_chunkAllocate(glm::ivec3(1, 0, 1));
    voxel_chunkAllocate(glm::ivec3(1, 1, 1));
    voxel_chunkAllocate(glm::ivec3(-1, 0, 0));


    for (int i = 0; i < chunkData.size(); i++) {
        Chunk chunk = chunkData[i];
        chunk.data.index;
        for (int j = 0; j < chunk.data.size.x*chunk.data.size.y*chunk.data.size.z; j++) {
            int number = j;
            int z = number/(64*64);
            number %= 64*64;
            int y = number/64;
            int x = number%64;

            voxelData[chunk.data.index+j].data = (VOXELSOLID*(x%2)*(y%2)*(z%2)) | (toColor(z)<<10) | (toColor(y)<<5) | toColor(x);
        }
    }

    gpubuffers_init();
    gpubuffers_upload();
}

void engine_cleanup() {
    gpubuffers_cleanup();
    graphics_cleanup();
    video_cleanup();
}

void engine_update() {
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