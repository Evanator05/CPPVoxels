#include "stdio.h"

#include "e_engine.h"
#include "i_video.h"
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

void engine_init() {
    video_init();
    graphics_init();
    voxel_init();
    
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

void engine_update() {

    worldInfo.cameraPos = glm::vec3(
        sin(worldInfo.time/20)*(sin(worldInfo.time/5)+1)*128+128,
        150,
        cos(worldInfo.time/20)*(sin(worldInfo.time/5)+1)*128+128
    );
    worldInfo.cameraRot = glm::vec2(worldInfo.time/20+3.14,.2);
    worldInfo_update();
    printf("XYZ %0.2f %0.2f %0.2f PY %0.2f %0.2f T %0.2f\n", worldInfo.cameraPos.x, worldInfo.cameraPos.y, worldInfo.cameraPos.z, worldInfo.cameraRot.x, worldInfo.cameraRot.y, worldInfo.time);

    // for (int i = 0; i < 1; i++) {
    //     voxelData[rand32()%voxelData.size()].data = rand() | (VOXELSOLID*(rand()&1));
    // }
    // gpubuffers_upload();
    
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