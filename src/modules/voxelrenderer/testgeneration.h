#include "modules/voxel/voxel.h"
#include "modules/voxel/voxelmanager.h"
#include "FastNoiseLite.h"
#include <string>
#include <math.h>
#include "FastNoiseLite.h"
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <random>

namespace Generator {
    void GenerateWorld(VoxelManager &vm);
    void GenerateCaves(VoxelManager &vm);
}   
