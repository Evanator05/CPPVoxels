#include "voxelphysics.h"
#include "modules/voxel/voxelmanager.h"

#include <glm/glm.hpp>
#include <cstdint>
#include <limits>
#include <span>

namespace {
#define TRACE_DIRECTION_EPSILON 1e-8f
#define TRACE_HUGE 1e30f
#define TRACE_NODE_WIDTH 4

    struct AABB {
        glm::ivec3 pos;
        glm::uvec3 size;
    };

    struct AABBIntersection {
        bool hit;
        float nearDepth;
        float farDepth;
        glm::vec3 t0;
        glm::vec3 t1;
    };

    struct PreparedRay {
        glm::vec3 origin;
        glm::vec3 direction;
        glm::vec3 invDirection;
        glm::ivec3 step;
    };

    float Min3(glm::vec3 v) { return glm::min(v.x, glm::min(v.y, v.z)); }
    float Max3(glm::vec3 v) { return glm::max(v.x, glm::max(v.y, v.z)); }

    std::uint32_t EncodeAxisMask(glm::bvec3 m) {
        return (m.x ? 1u : 0u) | (m.y ? 2u : 0u) | (m.z ? 4u : 0u);
    }

    glm::bvec3 DecodeAxisMask(std::uint32_t m) {
        return glm::bvec3((m & 1u) != 0u, (m & 2u) != 0u, (m & 4u) != 0u);
    }

    std::uint32_t MinAxisMask(glm::vec3 v) { return EncodeAxisMask(glm::lessThanEqual(v, glm::vec3(Min3(v)))); }

    std::uint32_t CellIndex4(glm::ivec3 p) { return std::uint32_t(p.x + 4*p.y + 16*p.z); }

    bool IsOutsideNode(glm::ivec3 p) {
        return glm::any(glm::lessThan(p, glm::ivec3(0))) || glm::any(glm::greaterThanEqual(p, glm::ivec3(4)));
    }

    RayQuery TraceMiss(glm::vec3 direction) {
        RayQuery r;
        r.hit = false;
        r.position = glm::ivec3(0);
        r.depth = (std::numeric_limits<float>::max)();
        r.normal = direction;
        r.voxel = Voxel{};
        return r;
    }

    glm::vec3 TraceVoxelNearTimes(glm::ivec3 position, PreparedRay ray) {
        glm::vec3 front = glm::mix(glm::vec3(position + 1), glm::vec3(position), glm::greaterThan(ray.step, glm::ivec3(0)));
        glm::vec3 nearT = (front - ray.origin) * ray.invDirection;
        return glm::mix(nearT, glm::vec3(-TRACE_HUGE), glm::equal(ray.step, glm::ivec3(0)));
    }

    void TraceRefineHit(RayQuery& hit, PreparedRay ray, float minimumDepth) {
        glm::vec3 nearT = TraceVoxelNearTimes(hit.position, ray);
        float voxelEntry = Max3(nearT);
        hit.depth = glm::max(voxelEntry, minimumDepth);
        if (voxelEntry > 0.0f && voxelEntry >= minimumDepth) {
            hit.normal = -glm::vec3(ray.step) * glm::vec3(glm::greaterThanEqual(nearT, glm::vec3(voxelEntry)));
        }
    }

    PreparedRay PrepareRay(Ray ray) {
        PreparedRay r;
        glm::bvec3 parallel = glm::lessThan(glm::abs(ray.direction), glm::vec3(TRACE_DIRECTION_EPSILON));
        r.origin = ray.origin;
        r.direction = glm::mix(ray.direction, glm::vec3(0.0f), parallel);
        r.step = glm::ivec3(glm::sign(r.direction));
        glm::vec3 safeDirection = glm::mix(r.direction, glm::vec3(1.0f), parallel);
        r.invDirection = glm::mix(1.0f / safeDirection, glm::vec3(TRACE_HUGE), parallel);
        return r;
    }

    AABBIntersection IntersectAABBPrepared(PreparedRay ray, AABB aabb) {
        AABBIntersection r;
        glm::vec3 lo = glm::vec3(aabb.pos);
        glm::vec3 hi = lo + glm::vec3(aabb.size);
        glm::bvec3 parallel = glm::equal(ray.step, glm::ivec3(0));

        bool outside = glm::any(parallel && ((glm::lessThan(ray.origin, lo)) || (glm::greaterThanEqual(ray.origin, hi))));
        r.t0 = glm::mix((lo - ray.origin) * ray.invDirection, glm::vec3(-TRACE_HUGE), parallel);
        r.t1 = glm::mix((hi - ray.origin) * ray.invDirection, glm::vec3( TRACE_HUGE), parallel);
        r.nearDepth = glm::max(Max3(glm::min(r.t0, r.t1)), 0.0f);
        r.farDepth = Min3(glm::max(r.t0, r.t1));
        r.hit = !outside && glm::any(glm::notEqual(ray.step, glm::ivec3(0))) && glm::all(glm::greaterThan(aabb.size, glm::uvec3(0))) && r.farDepth >= r.nearDepth;
        return r;
    }

    AABBIntersection IntersectAABB(Ray ray, AABB aabb) {
        return IntersectAABBPrepared(PrepareRay(ray), aabb);
    }

    struct TraceNodeMasks {
        glm::uvec2 solid;
    };

    TraceNodeMasks TraceLoadMasks(std::uint32_t node, std::span<const ContreeHeader> headers) {
        TraceNodeMasks m;
        std::uint64_t solid = headers[node].isSolidMask;
        m.solid = glm::uvec2(std::uint32_t(solid), std::uint32_t(solid >> 32));
        return m;
    }

    bool TraceIsVoxel(std::uint32_t node, std::uint32_t slot, std::span<const ContreeHeader> headers) {
        // Fetch the voxel/child discriminator only after the solid bit is set.
        std::uint64_t voxel = headers[node].isVoxelMask;
        std::uint32_t word = slot < 32u ? std::uint32_t(voxel) : std::uint32_t(voxel >> 32);
        return ((word >> (slot & 31u)) & 1u) != 0;
    }

    bool TraceMaskBit(glm::uvec2 mask, std::uint32_t slot) {
        // Storage remains std::uint64_t; bounded 32-bit shifts select either half.
        std::uint32_t word = slot < 32u ? mask.x : mask.y;
        return ((word >> (slot & 31u)) & 1u) != 0u;
    }

    bool TraceEmptyGroup(glm::uvec2 solid, std::uint32_t slot) {
        // Eight children in the aligned 2x2x2 group containing this slot.
        std::uint32_t word = slot < 32u ? solid.x : solid.y;
        return ((word >> (slot & 0x0Au)) & 0x00330033u) == 0u;
    }

    std::uint32_t TracePositionSlot(glm::uvec3 positionBits, std::uint32_t scaleExp, std::uint32_t mirrorMask) {
        glm::uvec3 c = (positionBits >> scaleExp) & glm::uvec3(3u);
        return (c.x | (c.y << 2) | (c.z << 4)) ^ mirrorMask;
    }
    RayQuery TraceChunkAtEntry(std::uint32_t rootNode, PreparedRay ray, glm::vec3 entryPosition, float startDepth, std::uint32_t entryMask, std::span<const ContreeHeader> contreeHeaders, std::span<const ContreeData> contreeData, float maxDepth = (std::numeric_limits<float>::max)(), glm::ivec3 coordinateOffset = glm::ivec3(0)) {
        RayQuery result = TraceMiss(ray.direction);
        if (!(maxDepth >= 0.0f) ||
            (maxDepth < (std::numeric_limits<float>::max)() && !(startDepth <= maxDepth)) ||
            rootNode == POINTER_EMPTY || glm::all(glm::equal(ray.step, glm::ivec3(0)))) return result;

        TraceNodeMasks masks = TraceLoadMasks(rootNode, contreeHeaders);
        if ((masks.solid.x | masks.solid.y) == 0u) return result;

        glm::vec3 entryFloor = glm::floor(entryPosition);
        glm::ivec3 entryVoxel = glm::ivec3(entryFloor) - glm::ivec3((glm::lessThan(ray.step, glm::ivec3(0))) && (glm::equal(entryPosition, entryFloor)));
        entryVoxel = glm::clamp(entryVoxel, glm::ivec3(0), glm::ivec3(63));
        std::uint32_t node = rootNode;
        std::uint32_t middleNode = rootNode;
        std::uint32_t scaleExp = 21u;

        glm::bvec3 mirrored = glm::greaterThan(ray.step, glm::ivec3(0));
        std::uint32_t mirrorMask = (mirrored.x ? 3u : 0u) |
                          (mirrored.y ? 12u : 0u) |
                          (mirrored.z ? 48u : 0u);

        glm::vec3 scaledEntry = entryPosition * (1.0f / 64.0f);
        glm::vec3 walkOrigin = glm::mix(1.0f + scaledEntry, 2.0f - scaledEntry, mirrored);
        walkOrigin = glm::clamp(walkOrigin, glm::vec3(1.0f), glm::vec3(2.0f));
        glm::vec3 walkDirection = -glm::abs(ray.direction) * (1.0f / 64.0f);
        glm::vec3 walkInverse = -glm::abs(ray.invDirection) * 64.0f;

        glm::uvec3 mirroredVoxel = glm::uvec3(entryVoxel) ^ glm::mix(glm::uvec3(0u), glm::uvec3(63u), mirrored);
        glm::uvec3 entryLower = glm::uvec3(0x3F800000u) | (mirroredVoxel << 17u);
        glm::uvec3 positionBits = glm::clamp(glm::floatBitsToUint(walkOrigin), entryLower, entryLower | glm::uvec3(0x0001FFFFu));

        float travel = 0.0f;

        for (std::uint32_t iteration = 0u; iteration < 192u; ++iteration) {
            if (maxDepth < (std::numeric_limits<float>::max)() && startDepth + travel > maxDepth) {
                glm::uvec3 voxelPos = ((positionBits >> 17u) & glm::uvec3(63u)) ^
                    glm::mix(glm::uvec3(0u), glm::uvec3(63u), mirrored);
                float voxelEntry = Max3(TraceVoxelNearTimes(glm::ivec3(voxelPos) + coordinateOffset, ray));
                if (glm::max(voxelEntry, startDepth) > maxDepth) break;
            }
            std::uint32_t slot = TracePositionSlot(positionBits, scaleExp, mirrorMask);

            while (TraceMaskBit(masks.solid, slot)) {
                bool voxel = scaleExp == 17u;
                voxel = voxel || TraceIsVoxel(node, slot, contreeHeaders);
                if (voxel) {
                    glm::uvec3 voxelPos = (positionBits >> 17u) & glm::uvec3(63u);
                    voxelPos ^= glm::mix(glm::uvec3(0u), glm::uvec3(63u), mirrored);
                    float hitDepth = startDepth + travel;
                    if (maxDepth < (std::numeric_limits<float>::max)()) {
                        hitDepth = glm::max(startDepth,
                            Max3(TraceVoxelNearTimes(glm::ivec3(voxelPos) + coordinateOffset, ray)));
                        if (!(hitDepth <= maxDepth)) return result;
                    }
                    std::uint32_t value = contreeData[node].child_nodes[slot];
                    result.hit = true;
                    result.position = glm::ivec3(voxelPos);
                    result.depth = hitDepth;
                    result.normal = -glm::vec3(ray.step) * glm::vec3(DecodeAxisMask(entryMask));
                    result.voxel = Voxel{value};
                    return result;
                }

                std::uint32_t nextNode = contreeData[node].child_nodes[slot];
                if (nextNode == POINTER_EMPTY) break;
                if (scaleExp == 19u) middleNode = node;
                node = nextNode;
                scaleExp -= 2u;
                masks = TraceLoadMasks(node, contreeHeaders);
                slot = TracePositionSlot(positionBits, scaleExp, mirrorMask);
            }

            std::uint32_t advanceExp = scaleExp;
            bool skipNode = (masks.solid.x | masks.solid.y) == 0u;
            glm::uvec3 nodeBoxBits;
            glm::vec3 nodeExits;
            if (skipNode) {
                nodeBoxBits = positionBits & glm::uvec3(0xFFFFFFFFu << (scaleExp + 2u));
                nodeExits = (glm::uintBitsToFloat(nodeBoxBits) - walkOrigin) * walkInverse;
                nodeExits = glm::mix(nodeExits, glm::vec3(TRACE_HUGE), glm::equal(ray.step, glm::ivec3(0)));
            }

            glm::uvec3 boxBits;
            glm::vec3 exits;
            glm::uvec3 advanceVector = glm::uvec3(advanceExp);

            if (skipNode) {
                advanceExp += 2u;
                boxBits = nodeBoxBits;
                exits = nodeExits;
            } else {

                if (TraceEmptyGroup(masks.solid, slot)) {
                    // Grow only through proven-empty children. Vector exponents
                    // let the same float-bit walk skip rectangular regions.
                    advanceVector += glm::uvec3(1u);
                    std::uint32_t combined = masks.solid.x | masks.solid.y;
                    std::uint32_t word = slot < 32u ? masks.solid.x : masks.solid.y;
                    bool halfY = ((combined >> (slot & 8u)) & 0x00FF00FFu) == 0;
                    bool halfX = ((combined >> (slot & 2u)) & 0x33333333u) == 0;
                    bool halfZ = word == 0;
                    if (halfY) advanceVector += glm::uvec3(1,0,1);
                    else if (halfX) advanceVector += glm::uvec3(0,1,1);
                    else if (halfZ) advanceVector += glm::uvec3(1,1,0);
                    else {
                        if (((word >> (slot & 8u)) & 0x00FF00FFu) == 0) advanceVector.x++;
                        else if (((word >> (slot & 2u)) & 0x33333333u) == 0) advanceVector.y++;
                        else if (((combined >> (slot & 10u)) & 0x00330033u) == 0) advanceVector.z++;
                    }
                }
                boxBits = positionBits & (glm::uvec3(0xFFFFFFFFu) << advanceVector);

                exits = (glm::uintBitsToFloat(boxBits) - walkOrigin) * walkInverse;
                exits = glm::mix(exits, glm::vec3(TRACE_HUGE), glm::equal(ray.step, glm::ivec3(0)));
            }
            float nextTravel = Min3(exits);
            glm::bvec3 crossed = glm::lessThanEqual(exits, glm::vec3(nextTravel));
            travel = glm::max(nextTravel, travel);
            entryMask = EncodeAxisMask(crossed);

            if (skipNode) advanceVector = glm::uvec3(advanceExp);
            glm::uvec3 tail = (glm::uvec3(1u) << advanceVector) - glm::uvec3(1u);
            glm::uvec3 upperBits = boxBits + glm::mix(tail, glm::uvec3(0xFFFFFFFFu), crossed);
            glm::vec3 nextPosition = glm::min(walkOrigin + walkDirection * travel, glm::uintBitsToFloat(upperBits));
            glm::uvec3 nextBits = glm::floatBitsToUint(nextPosition);

            glm::bvec3 unitBoundary = glm::equal(nextBits & glm::uvec3(0x0001FFFFu), glm::uvec3(0u));
            nextBits -= glm::uvec3(unitBoundary && (glm::notEqual(ray.step, glm::ivec3(0))));
            glm::uvec3 diff = positionBits ^ nextBits;
            std::uint32_t changed = diff.x | diff.y | diff.z;
            positionBits = nextBits;

            if ((changed & 0xFF800000u) != 0u) break;

            if (scaleExp < 21u && (changed & 0x00600000u) != 0u) {
                node = rootNode;
                scaleExp = 21u;
                // Reload on ascent instead of retaining ancestor masks per ray.
                masks = TraceLoadMasks(node, contreeHeaders);
            } else if (scaleExp < 19u && (changed & 0x00180000u) != 0u) {
                node = middleNode;
                scaleExp = 19u;
                masks = TraceLoadMasks(node, contreeHeaders);
            }
        }
        return result;
    }

    RayQuery TraceChunkPrepared(Chunk chunk, PreparedRay worldRay, float startDepth, glm::bvec3 entryMask, std::span<const ContreeHeader> contreeHeaders, std::span<const ContreeData> contreeData, float maxDepth = (std::numeric_limits<float>::max)()) {
        PreparedRay localRay = worldRay;
        localRay.origin -= glm::vec3(chunk.position) * 64.0f;
        glm::vec3 entryPosition = localRay.origin + localRay.direction * startDepth;
        RayQuery result = TraceChunkAtEntry(chunk.contree_node, localRay, entryPosition, startDepth, EncodeAxisMask(entryMask), contreeHeaders, contreeData, maxDepth);
        if (result.hit) {
            TraceRefineHit(result, localRay, startDepth);
            if (maxDepth < (std::numeric_limits<float>::max)() && !(result.depth <= maxDepth))
                return TraceMiss(worldRay.direction);
        }
        return result;
    }

    RayQuery TraceChunk(Chunk chunk, Ray ray, float startDepth, glm::bvec3 entryMask, std::span<const ContreeHeader> contreeHeaders, std::span<const ContreeData> contreeData, float maxDepth = (std::numeric_limits<float>::max)()) {
        return TraceChunkPrepared(chunk, PrepareRay(ray), startDepth, entryMask, contreeHeaders, contreeData, maxDepth);
    }

    RayQuery TraceWorld(Ray ray, std::span<const ContreeHeader> contreeHeaders, std::span<const ContreeData> contreeData, std::span<const Chunk> chunks, std::span<const ChunkPositionsHeader> chunkPositionsHeader, std::span<const std::uint32_t> chunkPositions, float maxDepth = (std::numeric_limits<float>::max)()) {
        RayQuery result = TraceMiss(ray.direction);
        if (!(maxDepth >= 0.0f)) return result;
        PreparedRay prepared = PrepareRay(ray);
        glm::ivec3 regionPosition = chunkPositionsHeader[0].position;
        glm::uvec3 regionSizeU = chunkPositionsHeader[0].size;
        if (glm::any(glm::equal(regionSizeU, glm::uvec3(0u)))) return result;
        glm::ivec3 regionSize = glm::ivec3(regionSizeU);
        glm::vec3 regionOrigin = glm::vec3(regionPosition) * 64.0f;

        AABB bounds;
        bounds.pos = regionPosition * 64;
        bounds.size = regionSizeU * 64u;
        AABBIntersection intersection = IntersectAABBPrepared(prepared, bounds);
        if (!intersection.hit || intersection.farDepth <= intersection.nearDepth ||
            (maxDepth < (std::numeric_limits<float>::max)() && intersection.nearDepth > maxDepth)) return result;

        glm::vec3 entryPosition = prepared.origin + prepared.direction * intersection.nearDepth;
        glm::vec3 gridPosition = (entryPosition - regionOrigin) * (1.0f / 64.0f);
        glm::ivec3 cell = glm::ivec3(glm::floor(gridPosition));
        cell -= glm::ivec3((glm::lessThan(prepared.step, glm::ivec3(0))) && (glm::equal(gridPosition, glm::vec3(cell))));
        cell = glm::clamp(cell, glm::ivec3(0), regionSize - 1);

        glm::vec3 delta = glm::abs(prepared.invDirection) * 64.0f;
        glm::vec3 boundary = regionOrigin + glm::vec3(cell + glm::ivec3(glm::greaterThan(prepared.step, glm::ivec3(0)))) * 64.0f;
        glm::vec3 nextBoundary = glm::max((boundary - prepared.origin) * prepared.invDirection, glm::vec3(intersection.nearDepth));
        nextBoundary = glm::mix(nextBoundary, glm::vec3(TRACE_HUGE), glm::equal(prepared.step, glm::ivec3(0)));

        glm::vec3 slabEntry = glm::min(intersection.t0, intersection.t1);
        std::uint32_t entryMask = EncodeAxisMask(glm::equal(slabEntry, glm::vec3(intersection.nearDepth)));
        float entry = intersection.nearDepth;
        std::uint32_t width = regionSizeU.x;
        std::uint32_t area = regionSizeU.x * regionSizeU.y;
        std::uint32_t maxSteps = regionSizeU.x + regionSizeU.y + regionSizeU.z;

        for (std::uint32_t iteration = 0u; iteration < maxSteps; ++iteration) {
            if (glm::any(glm::lessThan(cell, glm::ivec3(0))) || glm::any(glm::greaterThanEqual(cell, regionSize)) ||
                entry >= intersection.farDepth || (maxDepth < (std::numeric_limits<float>::max)() && entry > maxDepth)) break;

            std::uint32_t index;
            index = std::uint32_t(cell.x) + std::uint32_t(cell.y) * width + std::uint32_t(cell.z) * area;

            std::uint32_t chunkIndex = chunkPositions[index];
            if (chunkIndex != POINTER_EMPTY) {
                std::uint32_t rootNode = chunks[chunkIndex].contree_node;
                glm::ivec3 chunkPosition = regionPosition + cell;
                glm::vec3 localOrigin = prepared.origin - glm::vec3(chunkPosition) * 64.0f;
                glm::vec3 localEntry = localOrigin + prepared.direction * entry;
                RayQuery hit = TraceChunkAtEntry(rootNode, prepared, localEntry, entry, entryMask, contreeHeaders, contreeData, maxDepth, chunkPosition * 64);
                if (hit.hit) {
                    hit.position += chunkPosition * 64;
                    TraceRefineHit(hit, prepared, entry);
                    if (maxDepth < (std::numeric_limits<float>::max)() && !(hit.depth <= maxDepth)) return result;
                    return hit;
                }
            }

            float nextEntry = Min3(nextBoundary);
            if (maxDepth < (std::numeric_limits<float>::max)() && nextEntry > maxDepth) break;
            glm::bvec3 crossed = glm::lessThanEqual(nextBoundary, glm::vec3(nextEntry));
            cell += glm::ivec3(crossed) * prepared.step;
            entry = nextEntry;
            entryMask = EncodeAxisMask(crossed);
            nextBoundary += glm::vec3(crossed) * delta;
        }
        return result;
    }

    glm::vec2 GetNDC(glm::ivec2 pos, glm::ivec2 size) {
        glm::vec2 uv = (glm::vec2(pos) + 0.5f) / glm::vec2(size);
        glm::vec2 ndc = uv * 2.0f - 1.0f;
        ndc.y = -ndc.y;
        ndc.x *= float(size.x) / float(size.y);
        return ndc;
    }
}

RayQuery VoxelPhysics::TraceWorld(Ray ray, float maxDepth) {
    VoxelManager& manager = GetModule<VoxelManager>();
    if (manager.chunk_occupancy_needs_rebuild)
        manager.GenerateChunkOccupancyMap();

    const ChunkPositionsHeader header{manager.chunk_occupancy.position, manager.chunk_occupancy.size};
    return ::TraceWorld(ray, manager.contree_headers, manager.contree_data, manager.allocated_chunks,
        std::span<const ChunkPositionsHeader>(&header, 1),
        std::span<const std::uint32_t>(manager.chunk_occupancy.chunks, manager.chunk_occupancy.get_size()),
        maxDepth);
}
