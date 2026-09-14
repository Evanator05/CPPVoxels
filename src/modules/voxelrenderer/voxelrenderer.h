#pragma once

#include "engine.h"
#include "modules/renderer/renderer.h"
#include "modules/renderer/resources/buffer.h"
#include "glm/vec3.hpp"
#include "glm/mat3x3.hpp"
#include <glm/ext/matrix_relational.hpp>
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
            static constexpr uint32_t NORMAL_SHIFT = 0;
            static constexpr uint32_t NORMAL_MASK = 0x1Fu;

            static constexpr uint32_t ENABLED_MASK_SHIFT = 5;
            static constexpr uint32_t ENABLED_MASK = 1u << ENABLED_MASK_SHIFT;

            float red   = 0.0f;
            float green = 0.0f;
            float blue  = 0.0f;

            uint32_t flags = NORMAL_MASK;
            uint64_t solid = 0;

            void set_normal(uint8_t normal26) {
                uint32_t index = normal26 < 26u ? normal26 : 31u;
                flags = (flags & ~NORMAL_MASK) | (index << NORMAL_SHIFT);
            }
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
        void UpdateLightProbeSolid(uint32_t chunk_index, uint16_t probe_index);
        void UpdateLightProbeNormal(uint32_t chunk_index, uint16_t probe_index);

        uint8_t EncodeNormal26(glm::ivec3 normal);
        glm::ivec3 DecodeNormal26(uint8_t normal26);
        glm::ivec3 GetNormal(uint64_t solid, const uint64_t neighbors[6]);

    private:
        SDL_GPUDevice *device = nullptr;

        CameraTransform cameraTransform{};
        TypedBuffer<CameraTransform> *cameraTransformBuffer = nullptr;
        
        std::vector<LightChunk> lightChunks{};
        TypedBuffer<LightChunk> *lightChunksBuffer = nullptr;
};