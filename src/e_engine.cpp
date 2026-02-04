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

#include "i_loader.h"

bool running = true;

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
                        voxelData[offset + x + y*64 + z*64*64]->data = 0;
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
                        voxelData[offset + x + y*64 + z*64*64]->data = VOXELSOLID;
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

                    voxelData[offset + x + y*64 + z*64*64]->data = R | (G<<5) | (B<<10) | VOXELSOLID;
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

#include <unordered_map>

// helper functions for chunk math
inline int lfloor_div(int a, int b) {
    return (a >= 0) ? (a / b) : ((a - b + 1) / b);
}
inline int lfloor_mod(int a, int b)
{
    int r = a % b;
    return (r < 0) ? r + b : r;
}

inline int fast_floorf_to_int(float x) {
    int i = (int)x;
    return i - (i > x);
}

struct ivec3_hash {
    size_t operator()(const glm::ivec3& v) const {
        return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1) ^ (std::hash<int>()(v.z) << 2);
    }
};

void load_vox_file(const char *filename) {

    const ogt_vox_scene* scene;
    if (!loader_loadvoxfile(filename, scene)) return;

    std::unordered_map<glm::ivec3, uint32_t, ivec3_hash> chunkMap;

    for (uint32_t inst_idx = 0; inst_idx < scene->num_instances; inst_idx++) {
        const ogt_vox_instance* instance = &scene->instances[inst_idx];
        const ogt_vox_model* model = scene->models[instance->model_index];
        const ogt_vox_transform *transform = &scene->instances[inst_idx].transform;
        // Compute pivot (center of model)
        int pivotX = lfloor_div(model->size_x, 2);
        int pivotY = lfloor_div(model->size_y, 2);
        int pivotZ = lfloor_div(model->size_z, 2);

        uint32_t voxel_index = 0;
        for (uint32_t z = 0; z < model->size_z; z++) {
            for (uint32_t y = 0; y < model->size_y; y++) {
                for (uint32_t x = 0; x < model->size_x; x++, voxel_index++) {

                    uint8_t color_idx = model->voxel_data[voxel_index];
                    if (color_idx == 0) continue;

                    // Local voxel position relative to pivot
                    int lx = x - pivotX;
                    int ly = y - pivotY;
                    int lz = z - pivotZ;

                    // Apply MagicaVoxel rotation/axes (integer math)
                    float fx =
                        transform->m00 * (lx + 0.5f) +
                        transform->m10 * (ly + 0.5f) +
                        transform->m20 * (lz + 0.5f) +
                        transform->m30;

                    float fy =
                        transform->m01 * (lx + 0.5f) +
                        transform->m11 * (ly + 0.5f) +
                        transform->m21 * (lz + 0.5f) +
                        transform->m31;

                    float fz =
                        transform->m02 * (lx + 0.5f) +
                        transform->m12 * (ly + 0.5f) +
                        transform->m22 * (lz + 0.5f) +
                        transform->m32;

                    int mx = fast_floorf_to_int(fx);
                    int my = fast_floorf_to_int(fy);
                    int mz = fast_floorf_to_int(fz);

                    // Swap axes to engine space
                    int wx = mx;
                    int wy = mz; // MagicaVoxel Z -> engine Y
                    int wz = my; // MagicaVoxel Y -> engine Z

                    // Compute chunk coordinates using floor division (handles negatives correctly)
                    int cx = lfloor_div(wx, CHUNKWIDTH);
                    int cy = lfloor_div(wy, CHUNKWIDTH);
                    int cz = lfloor_div(wz, CHUNKWIDTH);
                    glm::ivec3 chunkPos(cx, cy, cz);

                    // Find or allocate chunk
                    uint32_t chunkIndex;
                    auto it = chunkMap.find(chunkPos);
                    if (it != chunkMap.end()) chunkIndex = it->second;
                    else {
                        chunkIndex = voxel_chunkAllocate(chunkPos);
                        chunkMap[chunkPos] = chunkIndex;
                    }

                    Chunk* c = &chunkData[chunkIndex];

                    // Compute voxel position **inside the chunk**
                    int lx_chunk = lfloor_mod(wx, CHUNKWIDTH);
                    int ly_chunk = lfloor_mod(wy, CHUNKWIDTH);
                    int lz_chunk = lfloor_mod(wz, CHUNKWIDTH);

                    uint32_t voxelIndexInChunk = lx_chunk + ly_chunk * CHUNKWIDTH + lz_chunk * CHUNKWIDTH * CHUNKWIDTH;
                    uint32_t startIndex = c->data.index;

                    // Pack voxel as solid + RGB555
                    const ogt_vox_rgba& col = scene->palette.color[color_idx];
                    uint16_t d =
                        VOXELSOLID |
                        ((col.r >> 3) & VOXELRED) |
                        (((col.g >> 3) & VOXELRED) << 5) |
                        (((col.b >> 3) & VOXELRED) << 10);

                    voxelData[startIndex + voxelIndexInChunk]->data = d;
                }
            }
        }
    }

    ogt_vox_destroy_scene(scene);
}

void engine_init() {
    video_init();
    graphics_init();
    gui_init();
    voxel_init();

    #define VOX
    #ifdef VOX
    worldInfo.cameraPos.y = 200;
    load_vox_file("minecraft.vox");
    #endif
    #ifdef NVOX
    for (int x = 0; x < 4; x++) {
        for (int z = 0; z < 4; z++) {
            for (int y = 0; y < 4; y++) {
                voxel_chunkAllocate(glm::ivec3(x, y, z));
            }
        }
    }
    gen_caves();
    #endif

    voxelData.dirty_all();

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

static int radius = 8;

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
        glm::ivec3 wp = glm::floor(worldInfo.cameraPos + (forward*16.0f));

        voxel_delete_sphere(wp, radius);

        gpubuffers_upload();
    }
}

int fps = 0;
void draw_fps_debug(float fps, float frameTimeMs)
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(200, 250), ImVec2(200, 250));
    ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

    // Color-coded FPS
    ImVec4 fpsColor;
    if (fps >= 60.0f)        fpsColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // green
    else if (fps >= 30.0f)   fpsColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // yellow
    else                     fpsColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // red

    ImGui::TextColored(fpsColor, "FPS: %.1f", fps);
    ImGui::Text("Frame Time: %02.2f ms", frameTimeMs);

    static float fpsHistory[120] = {0};
    static int idx = 0;
    fpsHistory[idx] = frameTimeMs;
    idx = (idx + 1) % IM_ARRAYSIZE(fpsHistory);

    ImGui::PlotLines("##", fpsHistory, IM_ARRAYSIZE(fpsHistory), idx, nullptr, 0.0f, 30.0f, ImVec2(130, 50));

    ImGui::Text("X: %02.2f", worldInfo.cameraPos.x);
    ImGui::Text("Y: %02.2f", worldInfo.cameraPos.y);
    ImGui::Text("Z: %02.2f", worldInfo.cameraPos.z);

    ImGui::Text("Chunks: %u", chunkData.size());

    ImGui::SliderInt("Radius", &radius, 1, 128);
    ImGui::SliderFloat("Speed", &MOVESPEED, 1, 1000);
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
        voxelData.dirty_all();
        gpubuffers_upload();
    }
    if (input_ispressed(SWAP_SCENE_2)) {
        voxelData.dirty_all();
        gpubuffers_upload();
    }
    if (input_ispressed(SWAP_SCENE_3)) {
        voxelData.dirty_all();
        gpubuffers_upload();
    }
    if (input_ispressed(SWAP_SCENE_4)) {
        voxelData.dirty_all();
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