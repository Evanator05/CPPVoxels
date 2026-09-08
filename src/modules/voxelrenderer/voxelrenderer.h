#pragma once

#include "engine.h"
#include "modules/renderer/renderer.h"
#include "modules/renderer/resources/buffer.h"
#include "glm/vec3.hpp"
#include "glm/mat3x3.hpp"
#include <glm/ext/matrix_relational.hpp> // Often required for matrix extensions
#include <glm/gtc/matrix_transform.hpp>
class VoxelRenderer : public EngineModule {
    public:
        using EngineModule::EngineModule;
        void Init(void) override;
        void Process(void) override;
        void Shutdown(void) override;

        struct CameraTransform
        {
            alignas(16) glm::ivec3 chunkPos;
            alignas(16) glm::vec3 localPos;

            alignas(16) glm::vec3 rotation0;
            alignas(16) glm::vec3 rotation1;
            alignas(16) glm::vec3 rotation2;
            alignas(16) float time = 0;
            int frame = 0;
        };

        struct alignas(16) FaceEntry {
            glm::vec<3, int32_t, glm::packed_highp> voxelPosition;
            uint32_t face;
            glm::vec<3, float, glm::packed_highp> indirectLighting;
            uint32_t frame;
        };

    private:
        SDL_GPUDevice *device = nullptr;
        TypedBuffer<CameraTransform> *posBuffer = nullptr;
        CameraTransform cameraTransform{};
};