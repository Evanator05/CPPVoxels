#include "audio.h"

#include "math.h"

#define SAMPLE_RATE 48000
#define FREQUENCY 440.0f

void Audio::Init() {
    if (!SDL_Init(SDL_INIT_AUDIO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return;
    }

    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.freq = SAMPLE_RATE;
    spec.format = SDL_AUDIO_F32;
    spec.channels = 1;

    int deviceCount;
    SDL_AudioDeviceID *devices = SDL_GetAudioPlaybackDevices(&deviceCount);
    
    audioDevice = SDL_OpenAudioDevice(devices[2], &spec);
    if (!audioDevice) {
        SDL_Log("Failed to open audio: %s", SDL_GetError());
        return;
    }

    audioStream = SDL_CreateAudioStream(&spec, &spec);
    if (!audioStream) {
        SDL_Log("Failed to create stream: %s", SDL_GetError());
        return;
    }
    SDL_BindAudioStream(audioDevice, audioStream);

    SDL_ResumeAudioDevice(audioDevice);

    bufferSamples = 2048;
    buffer = (float*)malloc(sizeof(float) * bufferSamples);
    phase = 0.0f;
}

void Audio::PreProcess() {

}

void Audio::Process() {
    int bytes_per_sample = sizeof(float);
    int target_bytes = bufferSamples * bytes_per_sample;

    int queued = SDL_GetAudioStreamAvailable(audioStream);

    if (queued < target_bytes) {

        for (int i = 0; i < bufferSamples; i++) {
            buffer[i] = sinf(phase * 2.0f * 3.14159265f);

            phase += FREQUENCY / SAMPLE_RATE;
            if (phase >= 1.0f)
                phase -= 1.0f;
        }

        SDL_PutAudioStreamData(
            audioStream,
            buffer,
            bufferSamples * bytes_per_sample
        );
    }
}

void Audio::Shutdown() {
    if (audioStream) {
        SDL_DestroyAudioStream(audioStream);
        audioStream = nullptr;
    }

    if (audioDevice) {
        SDL_CloseAudioDevice(audioDevice);
        audioDevice = 0;
    }

    if (buffer) {
        free(buffer);
        buffer = nullptr;
    }

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}