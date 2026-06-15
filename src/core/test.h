#pragma once
#include "engine.h"

#include "SDL3/SDL.h"

class Test : public EngineModule {
    public:
        using EngineModule::EngineModule;
        void Init(void) override;
        void Process(void) override;
        void Print(std::string output);
    private:

};