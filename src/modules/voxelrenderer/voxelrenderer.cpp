#include "voxelrenderer.h"
#include "modules/renderer/shaderpasses/imguipass.h"
#include "modules/renderer/shaderpasses/blitpass.h"
#include "shaders/test.h"
#include "window.h"

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
}

void VoxelRenderer::Process() {

}

void VoxelRenderer::Shutdown() {
    for (ShaderPass *shaderPass : shaderPasses) {
        shaderPass->Destroy();
    }
    for (Texture *texture : textures) {
        texture->Destroy();
    }
}
