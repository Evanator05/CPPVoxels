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

void Test::Init() {
    Console &console = GetModule<Console>();
    console.CreateCommand("print", [this](std::string output){
        Print(output);
    });

    VoxelManager &vm = GetModule<VoxelManager>();
    vm.AllocateChunk(glm::ivec3(1, 0, 0));
    vm.AllocateChunk(glm::ivec3(2, 0, 0));
    vm.AllocateChunk(glm::ivec3(7, 4, 0));
    vm.AllocateChunk(glm::ivec3(3, 0, 0));
    vm.AllocateChunk(glm::ivec3(4, 0, 0));
    vm.FreeChunk(2);
    vm.AllocateChunk(glm::ivec3(7, 4, 0));
    vm.GenerateChunkOccupancyMap();
    console.Log(std::to_string(vm.GetChunkDataAllocatedBytes()), Console::LogLevel::Info);
    console.Log(std::to_string(vm.GetChunkIndex(glm::ivec3(7, 4, 0))), Console::LogLevel::Info);
    console.Log(std::to_string(vm.GetChunkFromIndex(vm.GetChunkIndex(glm::ivec3(7, 4, 0)))->chunk_data_index), Console::LogLevel::Info);
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

