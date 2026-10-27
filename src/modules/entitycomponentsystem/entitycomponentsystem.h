#pragma once
#include "engine.h"

#include <vector>
#include <unordered_map>

#include <concepts>
#include <cstdint>
#include <type_traits>

#include "glm/vec3.hpp"

struct Entity {
    uint64_t id;
};

struct Component {};

template<typename T>
concept ComponentType = std::derived_from<T, Component> && !std::same_as<T, Component>;

struct Position : Component {
    glm::ivec3 position;
};

class ComponentPoolBase {
public:
    virtual ~ComponentPoolBase() = default;
    virtual void Remove(uint64_t entity_id) = 0;
};

template<ComponentType T>
class ComponentPool : public ComponentPoolBase {
public:
    std::unordered_map<uint64_t, T> components;

    T* Create(uint64_t entity_id) {
        auto [it, inserted] = components.try_emplace(entity_id);
        return &it->second;
    }

    T* Get(uint64_t entity_id) {
        auto it = components.find(entity_id);

        if (it == components.end())
            return nullptr;

        return &it->second;
    }

    void Remove(uint64_t entity_id) override {
        components.erase(entity_id);
    }
};


class EntityComponentSystem : public EngineModule {
    public:
        using EngineModule::EngineModule;
        void Init(void) override;
        void Process(void) override;
        void Shutdown(void) override;

        Entity *CreateEntity();
        void DestoryEntity(Entity *entity);

        template<ComponentType T>
        T *CreateComponent(Entity *entity);

        template<ComponentType T>
        T *GetComponent(Entity *entity);

        template<ComponentType T>
        void RemoveComponent(Entity *entity);

    private:
        uint64_t current_id = 0;

        std::unordered_map<uint64_t, Entity> entities;
        std::unordered_map<std::type_index, std::unique_ptr<ComponentPoolBase>> component_pools;

        template<ComponentType T>
        ComponentPool<T>* GetPool() {
            std::type_index type = typeid(T);

            auto it = component_pools.find(type);

            if (it != component_pools.end())
                return static_cast<ComponentPool<T>*>(it->second.get());

            auto pool = std::make_unique<ComponentPool<T>>();
            auto* ptr = pool.get();

            component_pools.emplace(type, std::move(pool));

            return ptr;
        }
};

template<ComponentType T>
T *EntityComponentSystem::CreateComponent(Entity *entity) {
    ComponentPool<T> *pool = GetPool<T>();
    return pool->Create(entity->id);
}

template<ComponentType T>
T *EntityComponentSystem::GetComponent(Entity * entity) {
    ComponentPool<T> *pool = GetPool<T>();
    return pool->Get(entity->id);
}

template<ComponentType T>
void EntityComponentSystem::RemoveComponent(Entity *entity) {
    auto it = component_pools.find(std::type_index(typeid(T)));
    if (it == component_pools.end())
        return;

    ComponentPool<T> *pool = GetPool<T>();
    pool->Remove(entity->id);
}