#pragma once

#include <unordered_map>
#include <typeindex>
#include <memory>
#include <vector>

class Engine; 

class EngineModule {
    public:
        EngineModule(Engine *engine) { this->engine = engine; }
        virtual ~EngineModule() = default;
        
        virtual void Init(void) {};
        virtual void PreProcess(void) {};
        virtual void Process(void) {};
        virtual void PostProcess(void) {};
        virtual void Shutdown(void) {};

        template<typename T>
        T& GetModule();
    protected:
        Engine *engine = nullptr;
};

class Engine {
    public:
        Engine();
        ~Engine();

        void Init(void);
        void Process(void);
        void Shutdown(void);

        void Run(void);
        void Quit(void);

        template<typename T, typename... Args>
        T& AddModule(Args&&... args);

        template<typename T>
        T& GetModule() {
            return *static_cast<T*>(modules.at(typeid(T)).get());
        }
        
    private:
        std::unordered_map<std::type_index, std::unique_ptr<EngineModule>> modules;
        std::vector<EngineModule*> moduleOrder;
        bool running = true;
        double timer = 0.0;
};
template<typename T>
T& EngineModule::GetModule() {
    return engine->GetModule<T>();
}