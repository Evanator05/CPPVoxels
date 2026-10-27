#include "entitycomponentsystem.h"

Entity* EntityComponentSystem::CreateEntity() {
    uint64_t id = current_id++;
    auto [it, inserted] = entities.emplace(id, Entity{id});
    return &it->second;
}

void EntityComponentSystem::DestoryEntity(Entity *entity) {
    if (!entity)
        return;

    uint64_t id = entity->id;
    
    for (auto& [type, pool] : component_pools) {
        pool->Remove(id);
    }

    entities.erase(id);
}