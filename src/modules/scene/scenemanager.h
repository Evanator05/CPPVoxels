#pragma once
#include "engine.h"

#include "gameobject.h"

#include <vector>
#include <unordered_map>

class SceneManager : public EngineModule {
    public:
        using EngineModule::EngineModule;
        void Init(void) override;
        void Process(void) override;
        void Shutdown(void) override;

        template<typename T = GameObject>
        T* GetObject(ObjectHandle<T> id);

    private:
        std::unordered_map<uint64_t, GameObject**> object_ids;
        std::vector<GameObject*> objects;
};