#include "voxelrenderer.h"
#include "modules/renderer/shaderpasses/imguipass.h"
#include "modules/renderer/shaderpasses/blitpass.h"
#include "shaders/test.h"
#include "window.h"
#include "deltatime.h"
#include "input.h"
#include "console.h"

void VoxelRenderer::Init() {
    Window &window = GetModule<Window>();
    Renderer &renderer = GetModule<Renderer>();

    window.ResizedScreen.Bind(
        [this](glm::ivec2 size) {
            display->size = size;
            display->Create();
        }
    );

    device = renderer.GetDevice();
    
    display = new Texture(device);
    display->size = window.GetSize();
    display->usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    display->format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    display->Create();


    posBuffer = new TypedBuffer<float>(device);
    posBuffer->usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
    posBuffer->SetSize(3);
    posBuffer->Create();

    ComputePass *test = new ComputePass(device);
    test->spirv = test_spirv;
    test->spirv_size = std::size(test_spirv);
    test->threadcount = {16, 16, 1};
    test->readwrite_storage_textures.push_back(display);
    test->dispatchFunc = [this](const ComputePass& pass) {
        Window &w = GetModule<Window>();
        glm::ivec2 size = w.GetSize();
        return glm::uvec3(
            ((size.x)+pass.threadcount.x-1)/pass.threadcount.x,
            ((size.y)+pass.threadcount.y-1)/pass.threadcount.y,
            1
        );
    };
    test->readonly_storage_buffers.push_back(posBuffer);
    test->Create();
    
    BlitPass *copyToSwaptex = new BlitPass(device);
    copyToSwaptex->source = display;
    copyToSwaptex->destination = &renderer.swapchainTexture;

    ImGuiPass *gui = new ImGuiPass(device);
    gui->destination = &renderer.swapchainTexture;

    renderer.shaderPassOrder.push_back(test);
    renderer.shaderPassOrder.push_back(copyToSwaptex);
    renderer.shaderPassOrder.push_back(gui);

    // push to vectors for cleanup later
    shaderPasses.push_back(test);
    shaderPasses.push_back(copyToSwaptex);
    shaderPasses.push_back(gui);
    textures.push_back(display);

    pos[0] = 0;
    pos[1] = 6.5;
    pos[2] = 6.5;
}

void VoxelRenderer::Process() {
    Input &input = GetModule<Input>();
    float deltaTime = GetModule<DeltaTime>().Get()*10;
    if (input.IsHeld("break_block")) {
        deltaTime *= 5;
    }
    if (input.IsHeld("left")) {
        pos[0] += deltaTime;
    }
    if (input.IsHeld("right")) {
        pos[0] -= deltaTime;
    }
    if (input.IsHeld("forward")) {
        pos[2] += deltaTime;
    }
    if (input.IsHeld("backward")) {
        pos[2] -= deltaTime;
    }
    if (input.IsHeld("up")) {
        pos[1] += deltaTime;
    }
    if (input.IsHeld("down")) {
        pos[1] -= deltaTime;
    }
    posBuffer->Upload(pos, 3);
}

void VoxelRenderer::Shutdown() {
    for (ShaderPass *shaderPass : shaderPasses) {
        shaderPass->Destroy();
    }
    for (Texture *texture : textures) {
        texture->Destroy();
    }
}
