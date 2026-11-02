#pragma once
#include "engine.h"
#include "voxel/voxel.h"
#include "voxelparticle/immediateparticle.h"

/* 
    The particle manager is composed of 3 systems
    Emitters are sets of parameters to spawn particles like position and starting velocity
    Behaviors are how particles act (ie gravity and other forces being applied to them)
    Particles are the individual things being simulated they have a position velocity lifetime and color
*/


class VoxelParticleManager : public EngineModule {
    public:

    private:

};
