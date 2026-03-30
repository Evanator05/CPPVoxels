#include <stdexcept>
#include <ranges>

#include "engine.h"

#include "deltatime.h"
#include "window.h"
#include "input.h"
#include "audio.h"

Engine::Engine() {
    AddModule<DeltaTime>();
    AddModule<Input>();
    AddModule<Window>();
    AddModule<Audio>();
}

Engine::~Engine() {

}

void Engine::Init() {
    for (EngineModule *module : moduleOrder) {
        module->Init();
    }
}

void Engine::Process() {
    for (EngineModule *module : moduleOrder) {
        module->PreProcess();
    }
    for (EngineModule *module : moduleOrder) {
        module->Process();
    }
    for (EngineModule *module : moduleOrder) {
        module->PostProcess();
    }
}

void Engine::Shutdown() {
    for (EngineModule* module : std::views::reverse(moduleOrder)) {
        module->Shutdown();
    }
}

void Engine::Run() {
    Init();
    while (running)
        Process();
    Shutdown();
}

void Engine::Quit() {
    running = false;
}

template<typename T, typename... Args>
T& Engine::AddModule(Args&&... args) {
    static_assert(std::is_base_of<EngineModule, T>::value, "T must derive from EngineModule");

    auto type = std::type_index(typeid(T));

    if (modules.find(type) != modules.end()) {
        throw std::runtime_error("Module already exists");
    }

    auto module = std::make_unique<T>(this, std::forward<Args>(args)...);
    T* rawPtr = module.get();

    modules[type] = std::move(module);

    moduleOrder.push_back(rawPtr);

    return *rawPtr;
}