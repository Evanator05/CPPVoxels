#include "engine.h"

class VoxelManager : public EngineModule {
    public:
        using EngineModule::EngineModule;
        void Init(void) override;
        void Process(void) override;
        void PostProcess(void) override;
        void Shutdown(void) override;

        void SaveWorld(void);
        void LoadWorld(void);
    private:

};