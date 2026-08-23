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
        };

    private:
        SDL_GPUDevice *device = nullptr;
        TypedBuffer<CameraTransform> *posBuffer = nullptr;
        CameraTransform cameraTransform{};
};