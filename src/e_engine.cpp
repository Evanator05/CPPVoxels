#include "stdio.h"

#include "e_engine.h"
#include "i_video.h"
#include "i_input.h"
#include "i_graphics.h"
#include "voxel.h"
#include "i_gpubuffers.h"
#include "worldinfo.h"
#include "i_gui.h"

#include "time.h"
#include "stdlib.h"

#include "math.h"

bool running = true;

void gen_test_chunks(void) {
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
                voxelData[offset+j].data = (VOXELRED | rand()) | VOXELSOLID*(gy<terrainHeight || gy>(terrainHeight+96));
            } else {
                number = rand()%32;
                voxelData[offset+j].data = (number | number<<5 | number<<10) | VOXELSOLID*(gy<terrainHeight || gy>(terrainHeight+96));
            }
        }
    }
}

static inline int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline float sdf_sphere(float x, float y, float z, float r) {
    return (x*x + y*y + z*z) - r*r;
}

static inline float sdf_cylinder_y(float x, float z, float r) {
    return (x*x + z*z) - r*r;
}

static inline float sdf_torus(float x, float y, float z, float R, float r) {
    float qx = (x*x + z*z) - R*R;
    return (qx*qx + y*y) - r*r;
}

void gen_chunks_sdf_ball_carved(void) {
    const float BALL_RADIUS = 128.0f;
    const glm::vec3 CENTER = glm::vec3(128,128,128);
    const float EPS = 0.5f; // for numerical normal
    const float MAX_CARVER_RADIUS = 40.0f + 14.0f;
    const float BOUND = BALL_RADIUS + MAX_CARVER_RADIUS;

    for (int ci = 0; ci < chunkData.size(); ci++) {
        Chunk chunk = chunkData[ci];
        int offset = chunk.data.index;

        // --- slice caching buffers for previous, current, next z ---
        float slice_prev_storage[64][64], slice_curr_storage[64][64], slice_next_storage[64][64];
        float (*slice_prev)[64] = slice_prev_storage;
        float (*slice_curr)[64] = slice_curr_storage;
        float (*slice_next)[64] = slice_next_storage;

        // precompute first two slices
        auto full_sdf = [&](float x, float y, float z) -> float {
            float d = sqrtf(x*x + y*y + z*z) - BALL_RADIUS;
            d = fmaxf(d, -sqrtf(x*x + z*z) + 8.0f);
            d = fmaxf(d, -sqrtf(y*y + z*z) + 6.0f);
            d = fmaxf(d, -sqrtf(x*x + y*y) + 6.0f);
            float qx = sqrtf(x*x + z*z) - 40.0f;
            d = fmaxf(d, -sqrtf(qx*qx + y*y) + 5.0f);
            d = fmaxf(d, -sqrtf(x*x + (y-32)*(y-32) + z*z) + 14.0f);
            d = fmaxf(d, -sqrtf(x*x + (y+32)*(y+32) + z*z) + 14.0f);
            return d;
        };

        // compute first slice
        int z = 0;
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 64; x++) {
                float gx = x + chunk.pos.x * 64;
                float gy = y + chunk.pos.y * 64;
                float gz = z + chunk.pos.z * 64;
                float px = gx - CENTER.x;
                float py = gy - CENTER.y;
                float pz = gz - CENTER.z;

                float radial = sqrtf(px*px + py*py + pz*pz);
                if (radial > BOUND) slice_curr[x][y] = BOUND + 1.0f; // outside
                else slice_curr[x][y] = full_sdf(px, py, pz);
            }
        }

        // compute second slice
        if (64 > 1) {
            int z_next = 1;
            for (int y = 0; y < 64; y++) {
                for (int x = 0; x < 64; x++) {
                    float gx = x + chunk.pos.x * 64;
                    float gy = y + chunk.pos.y * 64;
                    float gz = z_next + chunk.pos.z * 64;
                    float px = gx - CENTER.x;
                    float py = gy - CENTER.y;
                    float pz = gz - CENTER.z;

                    float radial = sqrtf(px*px + py*py + pz*pz);
                    if (radial > BOUND) slice_next[x][y] = BOUND + 1.0f;
                    else slice_next[x][y] = full_sdf(px, py, pz);
                }
            }
        }

        // --- process all slices ---
        for (z = 0; z < 64; z++) {
            float (*slice_below)[64] = slice_prev;
            float (*slice_curr_ptr)[64] = slice_curr;
            float (*slice_above)[64] = slice_next;

            for (int y = 0; y < 64; y++) {
                for (int x = 0; x < 64; x++) {
                    float dC = slice_curr_ptr[x][y];
                    if (dC >= 0.0f) { voxelData[offset + x + y*64 + z*64*64].data = 0; continue; }

                    // 6-face neighbors from cached slices
                    float dXP = (x<63) ? slice_curr_ptr[x+1][y] : full_sdf((x+chunk.pos.x*64+1)-CENTER.x, (y+chunk.pos.y*64)-CENTER.y, (z+chunk.pos.z*64)-CENTER.z);
                    float dXN = (x>0) ? slice_curr_ptr[x-1][y] : full_sdf((x+chunk.pos.x*64-1)-CENTER.x, (y+chunk.pos.y*64)-CENTER.y, (z+chunk.pos.z*64)-CENTER.z);
                    float dYP = (y<63) ? slice_curr_ptr[x][y+1] : full_sdf((x+chunk.pos.x*64)-CENTER.x, (y+chunk.pos.y*64+1)-CENTER.y, (z+chunk.pos.z*64)-CENTER.z);
                    float dYN = (y>0) ? slice_curr_ptr[x][y-1] : full_sdf((x+chunk.pos.x*64)-CENTER.x, (y+chunk.pos.y*64-1)-CENTER.y, (z+chunk.pos.z*64)-CENTER.z);
                    float dZP = (z<63) ? slice_above[x][y] : full_sdf((x+chunk.pos.x*64)-CENTER.x, (y+chunk.pos.y*64)-CENTER.y, (z+chunk.pos.z*64+1)-CENTER.z);
                    float dZN = (z>0) ? slice_below[x][y] : full_sdf((x+chunk.pos.x*64)-CENTER.x, (y+chunk.pos.y*64)-CENTER.y, (z+chunk.pos.z*64-1)-CENTER.z);

                    // surface check
                    if (dXP<0 && dXN<0 && dYP<0 && dYN<0 && dZP<0 && dZN<0) {
                        voxelData[offset + x + y*64 + z*64*64].data = VOXELSOLID;
                        continue;
                    }

                    // normal
                    float nx = dXP - dXN;
                    float ny = dYP - dYN;
                    float nz = dZP - dZN;
                    float nl = sqrtf(nx*nx + ny*ny + nz*nz) + 1e-6f;
                    nx/=nl; ny/=nl; nz/=nl;

                    // color
                    int R = (int)(16 + ny*12);
                    int G = (int)(12 + nx*10);
                    int B = (int)(14 + nz*10);
                    int stripe = ((int)(x*0.25f + z*0.25f)) & 3;
                    R += stripe; G -= stripe;
                    R = R<0?0:(R>31?31:R);
                    G = G<0?0:(G>31?31:G);
                    B = B<0?0:(B>31?31:B);

                    voxelData[offset + x + y*64 + z*64*64].data = R | (G<<5) | (B<<10) | VOXELSOLID;
                }
            }

            // shift slices for next iteration
            float (*tmp)[64] = slice_prev;
            slice_prev = slice_curr;
            slice_curr = slice_next;
            slice_next = tmp;

            // compute next slice if exists
            int znext = z + 2;
            if (znext < 64) {
                for (int y=0; y<64; y++)
                    for (int x=0; x<64; x++) {
                        float gx = x + chunk.pos.x*64;
                        float gy = y + chunk.pos.y*64;
                        float gz = znext + chunk.pos.z*64;
                        float px = gx - CENTER.x;
                        float py = gy - CENTER.y;
                        float pz = gz - CENTER.z;
                        float radial = sqrtf(px*px + py*py + pz*pz);
                        if (radial > BOUND) slice_next[x][y] = BOUND+1.0f;
                        else slice_next[x][y] = full_sdf(px, py, pz);
                    }
            }
        }
    }
}

#include "FastNoiseLite.h"
void gen_caves(void) {
    const float EPS = 0.5f;
    const float AIR_THRESHOLD = 5.0f;
    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    srand(time(0));
    noise.SetSeed(rand());
    noise.SetFrequency(0.02f);

    for (int ci = 0; ci < chunkData.size(); ci++) {
        Chunk chunk = chunkData[ci];
        int offset = chunk.data.index;

        float slice_prev_storage[64][64], slice_curr_storage[64][64], slice_next_storage[64][64];
        float (*slice_prev)[64] = slice_prev_storage;
        float (*slice_curr)[64] = slice_curr_storage;
        float (*slice_next)[64] = slice_next_storage;

        auto calc_noise = [&](float gx, float gy, float gz) -> float {
            float n = noise.GetNoise(gx, gy, gz) * 20.0f;
            n -= sinf(gx*0.1f + gz*0.2f)*10.0f;
            n -= cosf(gy*0.08f + gx*0.15f)*8.0f;
            return n;
        };

        for (int y=0;y<64;y++)
            for (int x=0;x<64;x++) {
                float gx = x + chunk.pos.x*64;
                float gy = y + chunk.pos.y*64;
                float gz = 0 + chunk.pos.z*64;
                slice_curr[x][y] = calc_noise(gx,gy,gz);
            }
        for (int y=0;y<64;y++)
            for (int x=0;x<64;x++) {
                float gx = x + chunk.pos.x*64;
                float gy = y + chunk.pos.y*64;
                float gz = 1 + chunk.pos.z*64;
                slice_next[x][y] = calc_noise(gx,gy,gz);
            }

        for (int z=0; z<64; z++) {
            float (*slice_below)[64] = slice_prev;
            float (*slice_curr_ptr)[64] = slice_curr;
            float (*slice_above)[64] = slice_next;

            for (int y=0; y<64; y++) {
                for (int x=0; x<64; x++) {
                    float gx = x + chunk.pos.x*64;
                    float gy = y + chunk.pos.y*64;
                    float gz = z + chunk.pos.z*64;
                    float dC = slice_curr_ptr[x][y];
                    if (dC < AIR_THRESHOLD) {
                        voxelData[offset + x + y*64 + z*64*64].data = 0;
                        continue;
                    }

                    // 6-face neighbor check
                    float dXP = (x<63)?slice_curr_ptr[x+1][y]:calc_noise(x+chunk.pos.x*64+1,y+chunk.pos.y*64,z+chunk.pos.z*64);
                    float dXN = (x>0)?slice_curr_ptr[x-1][y]:calc_noise(x+chunk.pos.x*64-1,y+chunk.pos.y*64,z+chunk.pos.z*64);
                    float dYP = (y<63)?slice_curr_ptr[x][y+1]:calc_noise(x+chunk.pos.x*64,y+chunk.pos.y*64+1,z+chunk.pos.z*64);
                    float dYN = (y>0)?slice_curr_ptr[x][y-1]:calc_noise(x+chunk.pos.x*64,y+chunk.pos.y*64-1,z+chunk.pos.z*64);
                    float dZP = (z<63)?slice_above[x][y]:calc_noise(x+chunk.pos.x*64,y+chunk.pos.y*64,z+chunk.pos.z*64+1);
                    float dZN = (z>0)?slice_below[x][y]:calc_noise(x+chunk.pos.x*64,y+chunk.pos.y*64,z+chunk.pos.z*64-1);

                    if (dXP< AIR_THRESHOLD && dXN< AIR_THRESHOLD && dYP< AIR_THRESHOLD && dYN< AIR_THRESHOLD && dZP< AIR_THRESHOLD && dZN< AIR_THRESHOLD) {
                        voxelData[offset + x + y*64 + z*64*64].data = VOXELSOLID;
                        continue;
                    }

                    // normal
                    float nx = dXP - dXN;
                    float ny = dYP - dYN;
                    float nz = dZP - dZN;
                    float nl = sqrtf(nx*nx + ny*ny + nz*nz) + 1e-6f;
                    nx/=nl; ny/=nl; nz/=nl;

                    // --- Color: stone base ---
                    int R = 10 + rand()%2; // brownish gray
                    int G = 10 + rand()%1;
                    int B = 10 + rand()%6;

                    // --- Grass if normal points mostly up ---

                    if (-ny > 0.5f) {  // only upward-facing surfaces
                        float blend = (-ny - 0.5f)/0.5f; // 0..1

                        // grass shades
                        int green_base = 22 + (rand()%6);
                        int green_high = 28 + (rand()%4);
                        float g_noise = noise.GetNoise(gx*0.2f, gy*0.2f, gz*0.2f); // -1..1
                        g_noise = (g_noise + 1.0f) * 0.5f; // 0..1
                        int green = (int)(green_base*(1.0f-g_noise) + green_high*g_noise);

                        // smooth blend with stone
                        R = (int)(R*(1.0f-blend) + green*blend);
                        G = (int)(G*(1.0f-blend) + 6*blend);
                        B = (int)(B*(1.0f-blend) + 6*blend);

                        // optional tip highlight
                        int highlight = rand()%3;
                        G = (G + highlight > 31) ? 31 : G + highlight;
                    }

                    voxelData[offset + x + y*64 + z*64*64].data = R | (G<<5) | (B<<10) | VOXELSOLID;
                }
            }

            // shift slices
            float (*tmp)[64] = slice_prev;
            slice_prev = slice_curr;
            slice_curr = slice_next;
            slice_next = tmp;

            int znext = z+2;
            if (znext<64)
                for (int y=0;y<64;y++)
                    for (int x=0;x<64;x++) {
                        float gx = x + chunk.pos.x*64;
                        float gy = y + chunk.pos.y*64;
                        float gz = znext + chunk.pos.z*64;
                        slice_next[x][y] = calc_noise(gx,gy,gz);
                    }
        }
    }
}

void engine_init() {
    video_init();
    graphics_init();
    gui_init();
    voxel_init();
    
    // generate world
    for (int x = 0; x < 4; x++) {
        for (int z = 0; z < 4; z++) {
            for (int y = 0; y < 4; y++) {
                voxel_chunkAllocate(glm::ivec3(x, y, z));
            }
        }
    }

    gen_caves();

    voxel_calculateChunkOccupancy();
    gpubuffers_init();
    gpubuffers_upload();
    worldInfo_init();

    input_set_mouse_lock(true);
}

void engine_cleanup() {
    worldInfo_cleanup();
    gpubuffers_cleanup();
    gui_cleanup();
    graphics_cleanup();
    video_cleanup();
}

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

float MOVESPEED = 50.0f;

inline int floor_div(int a, int b) {
    return (a >= 0) ? a / b : ((a - (b - 1)) / b);
}

inline int floor_mod(int a, int b) {
    int r = a % b;
    return (r < 0) ? r + b : r;
}

void move_camera(double deltaTime) {
    if (input_ispressed(SPEEDUP)) {
        MOVESPEED += 5.0;
    }

    if (input_ispressed(SPEEDDOWN)) {
        MOVESPEED -= 5.0;
    }
    MOVESPEED = MOVESPEED < 0 ? 0 : MOVESPEED;

    glm::vec2 mouse_rel = input_getmouse_rel();
    mouse_rel *= 0.005f;
    if (input_get_mouse_lock()) {
        worldInfo.cameraRot += (mouse_rel);
    }
    
    // Clamp pitch to avoid flipping
    constexpr float MAX_PITCH = glm::radians(89.0f);
    worldInfo.cameraRot.y = glm::clamp(
        worldInfo.cameraRot.y,
        -MAX_PITCH,
        MAX_PITCH
    );

    // Direction vectors
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
    if (glm::dot(move, move) > 0.1f) {
        move = glm::normalize(move);
    } else move = glm::vec3(0.0f);

    // Apply movement
    worldInfo.cameraPos += move * MOVESPEED * (float)deltaTime;

    // block breaking
    if (input_isheld(BREAK_BLOCK)) {
        glm::ivec3 wp = glm::floor(worldInfo.cameraPos + forward);

        glm::ivec3 chunkspace = {
            floor_div(wp.x, CHUNKWIDTH),
            floor_div(wp.y, CHUNKWIDTH),
            floor_div(wp.z, CHUNKWIDTH)
        };

        glm::ivec3 cspace = {
            floor_mod(wp.x, CHUNKWIDTH),
            floor_mod(wp.y, CHUNKWIDTH),
            floor_mod(wp.z, CHUNKWIDTH)
        };

        // Bounds check chunk map
        if (chunkspace.x < chunkOccupancyMapData.min.x || chunkspace.x >= chunkOccupancyMapData.max.x + 1 ||
            chunkspace.y < chunkOccupancyMapData.min.y || chunkspace.y >= chunkOccupancyMapData.max.y + 1 ||
            chunkspace.z < chunkOccupancyMapData.min.z || chunkspace.z >= chunkOccupancyMapData.max.z + 1)
            return;

        glm::ivec3 csize = chunkOccupancyMapData.max - chunkOccupancyMapData.min + glm::ivec3(1);
        uint32_t chunkMapIndex =
            chunkspace.x +
            chunkspace.y * csize.x+
            chunkspace.z * csize.x * csize.y;

        auto occ = chunkOccupancyMapData.data[chunkMapIndex];

        // Chunk not present
        if (occ.flags == 0)
            return;

        uint32_t voxelIndex =
            chunkData[occ.index].data.index +
            cspace.x +
            cspace.y * CHUNKWIDTH +
            cspace.z * CHUNKWIDTH * CHUNKWIDTH;
        
        voxelData[voxelIndex].data = 0;
        gpubuffers_upload();
    }
}

int fps = 0;
void draw_fps_debug(float fps, float frameTimeMs)
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(150, 130), ImVec2(150, 130));
    ImGui::Begin("Performance", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

    // Color-coded FPS
    ImVec4 fpsColor;
    if (fps >= 60.0f)        fpsColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // green
    else if (fps >= 30.0f)   fpsColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // yellow
    else                     fpsColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // red

    ImGui::TextColored(fpsColor, "FPS: %.1f", fps);
    ImGui::Text("Frame Time: %02.2f ms", frameTimeMs);

    // Optional: simple FPS histograph
    static float fpsHistory[120] = {0};
    static int idx = 0;
    fpsHistory[idx] = frameTimeMs;
    idx = (idx + 1) % IM_ARRAYSIZE(fpsHistory);

    ImGui::PlotLines("##", fpsHistory, IM_ARRAYSIZE(fpsHistory), idx, nullptr, 0.0f, 30.0f, ImVec2(130, 50));

    ImGui::End();
}


void engine_update(double deltaTime) {
    input_beginframe();
    video_update();
    gui_begin_frame();

    // handle game logic here
    move_camera(deltaTime);

    if (input_ispressed(TOGGLE_MOUSE_LOCK)) {
        input_set_mouse_lock(!input_get_mouse_lock());
    }

    if (input_ispressed(SWAP_SCENE_1)) {
        gen_caves();
        gpubuffers_upload();
    }
    if (input_ispressed(SWAP_SCENE_2)) {
        gen_chunks_sdf_ball_carved();
        gpubuffers_upload();
    }
    if (input_ispressed(SWAP_SCENE_3)) {
        gen_test_chunks();
        gpubuffers_upload();
    }
    if (input_ispressed(SWAP_SCENE_4)) {
        gpubuffers_upload();
    }

    draw_fps_debug(fps, deltaTime*1000);

    // start rendering
    worldInfo_update();
    graphics_update();
    input_endframe();
}

void engine_loop() {
    uint64_t currentTicks = SDL_GetPerformanceCounter();
    uint64_t lastTicks = 0;
    double deltaTime = 0;
    double updateTimer = 0;
    int samples = 0;
    int fpsSum = 0;
    
    while(running) {
        lastTicks = currentTicks;
        currentTicks = SDL_GetPerformanceCounter();
        deltaTime = (double)(currentTicks-lastTicks)/(double)SDL_GetPerformanceFrequency();

        updateTimer += deltaTime;
        fpsSum += (unsigned int)(1/deltaTime);
        samples++;
        if (updateTimer > 0.1) {
            fps = fpsSum/samples;
            updateTimer = 0.0;
            samples = 0;
            fpsSum = 0;
        }
        
        engine_update(deltaTime);
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