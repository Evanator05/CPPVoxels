#pragma once
#include "engine.h"

#include "SDL3/SDL.h"

class DeltaTime : public EngineModule {
    public:
        using EngineModule::EngineModule;
        void Init(void) override;
        void Process(void) override;

        double Get(void);
        
        void SetTimescale(double timescale);
        double GetTimescale(void);

        void SetMaxFPS(double targetfps);
        double GetMaxFPS(void);

    private:
        uint64_t lastticks = 0;
        double deltatime = 0.0;
        double frequency = 0.0;
        double timescale = 1.0;
        double maxfps = 165.0;
};