#include "i_loader.h"
#include "voxel.h"
#include "SDL3/SDL.h"

#include <streambuf>
#include <istream>
#include <sstream>
#include <cstdint>


bool loader_loadvoxfile(const char *filename, const ogt_vox_scene *&scene) {
    SDL_IOStream *io = SDL_IOFromFile(filename, "rb");
    if (!io) {
        SDL_Log("Failed to open file: %s", SDL_GetError());
        return false;
    }

    int64_t size = SDL_GetIOSize(io);
    if (size < 0) {
        SDL_Log("Failed to get size: %s", SDL_GetError());
        return false;
    }

    uint8_t *buffer = (uint8_t*)SDL_malloc(size);
    int64_t bytesRead = SDL_ReadIO(io, buffer, size);
    if (bytesRead != size) {
        SDL_Log("Read error: %s", SDL_GetError());
        SDL_free(buffer);
        return false;
    }

    SDL_CloseIO(io);  

    scene = ogt_vox_read_scene(buffer, bytesRead);

    SDL_free(buffer);

    return true;
}