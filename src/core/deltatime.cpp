#include "deltatime.h"

void DeltaTime::Init() {
    lastticks = SDL_GetPerformanceCounter();
    frequency = static_cast<double>(SDL_GetPerformanceFrequency());
    deltatime = 0.0;
}

void DeltaTime::Process() {
    uint64_t currentticks = SDL_GetPerformanceCounter();
    deltatime = static_cast<double>(currentticks - lastticks) / frequency;

    const double targetFrameTime = 1.0 / maxfps;
    if (deltatime < targetFrameTime) {
        SDL_Delay(static_cast<Uint32>((targetFrameTime - deltatime) * 1000.0));
        currentticks = SDL_GetPerformanceCounter();
        deltatime = static_cast<double>(currentticks - lastticks) / frequency;
    }

    lastticks = currentticks;
}

double DeltaTime::Get() {
    return deltatime*timescale;
}

void DeltaTime::SetTimescale(double timescale) {
    this->timescale = timescale;
}
double DeltaTime::GetTimescale() {
    return timescale;
}

void DeltaTime::SetMaxFPS(double maxfps) {
    this->maxfps = maxfps;
}
double DeltaTime::GetMaxFPS() {
    return maxfps;
}