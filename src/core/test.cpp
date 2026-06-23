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
void printBinary(uint64_t value)
{
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
    uint32_t chunk_index = vm.AllocateChunk(glm::ivec3(0, 0, 0));
    vm.GenerateChunkOccupancyMap();
    Voxel v{};
    v.set_r(3);
    v.set_g(2);
    v.set_b(1);
    v.set_solid(true);
    vm.SetVoxel(glm::ivec3(256, 0, 0), v);
    console.Log(vm.DumpContreeGraph(0), Console::LogLevel::Info);
    // console.Log(std::to_string(v2.r()) + "R " + std::to_string(v2.g()) + "G " + std::to_string(v2.b()) + "B", Console::LogLevel::Info);
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

