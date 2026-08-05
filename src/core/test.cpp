#include "test.h"
#include "gui.h"
#include "input.h"
#include "console.h"
#include "modules/voxel/voxelmanager.h"
#include "stdio.h"

void Test::Print(std::string output) {
    Console &console = GetModule<Console>();
    console.Log(output, Console::LogLevel::Info);
}
void printBinary(uint64_t value) {
    for (int i = 63; i >= 0; i--) {
        printf("%llu", (value >> i) & 1ULL);
    }
    printf("\n");
}
void Test::Init() {
    Console &console = GetModule<Console>();
    console.CreateCommand("print", [this](std::string output){
        Print(output);
    });

    VoxelManager &vm = GetModule<VoxelManager>();
    Relptr<AllocatedChunksBase> chunk_index = vm.AllocateChunk(glm::ivec3(0, 0, 0));
    vm.GenerateChunkOccupancyMap();
    Voxel v{};
    v.set_r('R');
    v.set_g('G');
    v.set_b('B');
    v.set_solid(true);
    // for(int x = 0; x < 4; x++) {
    //    for(int y = 0; y < 4; y++) {
    //         for(int z = 0; z < 4; z++) { 
    //             vm.SetVoxel(glm::ivec3(x, y, z), v);
    //         }
    //     }
    // }
    //vm.SetVoxel(glm::ivec3(0), v);
    vm.FillVoxels(glm::ivec3(0), glm::ivec3(31), v);

    console.Log(vm.DumpContreeGraph(0), Console::LogLevel::Info);
    Voxel v2 = vm.GetVoxel(glm::ivec3(0,0,0));
    console.Log(v2.to_string(), Console::LogLevel::Info);
}

void Test::Process() {
    Input &input = GetModule<Input>();
    Console &console = GetModule<Console>();

    ImGui::Begin("Stats");
    ImGui::Text("testing");
    if (ImGui::Button("PRESS ME!")) {
        console.Log("pressed button", Console::LogLevel::Info);
    }
    ImGui::End();
    
    if (input.IsPressed("forward")) {
        console.Log("pressed", Console::LogLevel::Info);
    }
    if (input.IsReleased("forward")) {
        console.Log("released", Console::LogLevel::Info);
    }
}

