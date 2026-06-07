#include "test.h"
#include "gui.h"
#include "input.h"
#include "console.h"

#include "stdio.h"

void Test::Init() {

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
