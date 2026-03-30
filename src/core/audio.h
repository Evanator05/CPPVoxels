#pragma once

#include "engine.h"

#include "SDL3/SDL.h"

class Audio : public EngineModule {
    public:
        using EngineModule::EngineModule;
        void Init(void) override;
        void PreProcess(void) override;
        void Process(void) override;
        void Shutdown(void) override;
    private:
        SDL_AudioDeviceID audioDevice;
        SDL_AudioStream *audioStream = nullptr;
        float *buffer = nullptr;
        int bufferSamples = 0;
        float phase = 0.0f;
};