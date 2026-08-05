#include <vector>

#include "SDL3/SDL.h"
#include "SDL3/SDL_gpu.h"

#include "resource.h"
#include "shaderpass.h"

class RenderingDevice {
    public:
        RenderingDevice(SDL_GPUDevice* device)
        : device(device) {}
        ~RenderingDevice();
        void Create();
        void Destroy();

        void Process();

        template<typename T>
        Resource<T>* CreateResource() {
            Resource<T> *resource = new Resource<T>(device);
            resources.push_back(resource);
            return resource;
        }
    private:
        SDL_GPUDevice* device = nullptr;
        
        std::vector<IResource*> resources;
        std::vector<ShaderPass*> shaderPasses;
};