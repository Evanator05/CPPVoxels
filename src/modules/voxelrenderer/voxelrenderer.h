#pragma once

#include "engine.h"
#include "modules/renderer/renderer.h"
#include "modules/renderer/resources/buffer.h"
#include "glm/vec3.hpp"
#include "glm/mat3x3.hpp"
#include <glm/ext/matrix_relational.hpp> // Often required for matrix extensions
#include <glm/gtc/matrix_transform.hpp>

#include "modules/voxel/voxel.h"
class VoxelRenderer : public EngineModule {
    public:
        struct CameraTransform {
            alignas(16) glm::ivec3 chunkPos;
            alignas(16) glm::vec3 localPos;

            alignas(16) glm::vec3 rotation0;
            alignas(16) glm::vec3 rotation1;
            alignas(16) glm::vec3 rotation2;
            alignas(16) float time = 0;
            int frame = 0;
        };

        struct LightProbe {
            static constexpr uint32_t RED_MASK_SHIFT = 0;
            static constexpr uint32_t RED_MASK = 0b11111u;

            static constexpr uint32_t GREEN_MASK_SHIFT = 5;
            static constexpr uint32_t GREEN_MASK = RED_MASK << GREEN_MASK_SHIFT;

            static constexpr uint32_t BLUE_MASK_SHIFT = 10;
            static constexpr uint32_t BLUE_MASK = RED_MASK << BLUE_MASK_SHIFT;

            static constexpr uint32_t NORMAL_SHIFT = 15;
            static constexpr uint32_t NORMAL_MASK = RED_MASK << NORMAL_SHIFT;

            static constexpr uint32_t ENABLED_MASK_SHIFT = 20;
            static constexpr uint32_t ENABLED_MASK = 1u << ENABLED_MASK_SHIFT;

            uint32_t light;
            uint64_t solid;
        };

        struct LightChunk {
            glm::ivec3 position;
            LightProbe probes[Chunk::WIDTH * Chunk::WIDTH * Chunk::WIDTH / (4 * 4 * 4)];
        };
    
        using EngineModule::EngineModule;
        void Init(void) override;
        void Process(void) override;
        void Shutdown(void) override;

        void CreateLightChunks();
        void UpdateLightChunk(uint32_t chunk_index);
        void UpdateLightProbe(uint32_t chunk_index, uint16_t probe_index);

    private:
        SDL_GPUDevice *device = nullptr;

        CameraTransform cameraTransform{};
        TypedBuffer<CameraTransform> *cameraTransformBuffer = nullptr;
        
        std::vector<LightChunk> lightChunks{};
        TypedBuffer<LightChunk> *lightChunksBuffer = nullptr;
};