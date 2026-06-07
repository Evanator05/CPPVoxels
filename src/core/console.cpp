#include "console.h"
#include "gui.h"
#include "input.h"

#include "stdio.h"

#include <iomanip>
#include <sstream>

static std::string FormatTime(const std::chrono::system_clock::time_point& tp) {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm;

#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    std::ostringstream ss;
    ss << std::put_time(&tm, "%H:%M:%S");
    return ss.str();
}

void Console::Init() {
    Input &input = GetModule<Input>();
    input.CreateAction("toggleconsole");
    input.CreateBinding("toggleconsole", SDLK_GRAVE);
}

void Console::Process() {
    Input &input = GetModule<Input>();
    if (input.IsPressed("toggleconsole")) {
        visible = !visible;
    }
    
    if (!visible) return;

    ImGui::Begin("Console");

    if (ImGui::Button("Clear")) {
        messages.clear();
    }
    ImGui::SameLine();

    static bool autoScroll = true;
    ImGui::Checkbox("Auto-scroll", &autoScroll);

    ImGui::Separator();

    ImGui::BeginChild("ScrollRegion", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar);

    if (ImGui::BeginTable("ConsoleTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable)) {

    ImGui::TableSetupColumn("Time");
    ImGui::TableSetupColumn("Level");
    ImGui::TableSetupColumn("Message");
    ImGui::TableHeadersRow();

    for (const LogEntry& entry : messages) {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(FormatTime(entry.time).c_str());

        ImGui::TableSetColumnIndex(1);
        switch (entry.level) {
            case LogLevel::Info:    ImGui::TextUnformatted("INFO"); break;
            case LogLevel::Warning: ImGui::TextUnformatted("WARN"); break;
            case LogLevel::Error:   ImGui::TextUnformatted("ERROR"); break;
        }

        ImGui::TableSetColumnIndex(2);

        if (entry.level == LogLevel::Warning)
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", entry.message.c_str());
        else if (entry.level == LogLevel::Error)
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", entry.message.c_str());
        else
            ImGui::TextUnformatted(entry.message.c_str());
    }

    ImGui::EndTable();
}

    if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();

    ImGui::Separator();

    static char inputBuf[512] = { 0 };

    ImGui::Text("Command:");
    ImGui::SameLine();

    ImGui::PushItemWidth(-1);

    bool enterPressed = ImGui::InputText(
        "##cmd",
        inputBuf,
        IM_ARRAYSIZE(inputBuf),
        ImGuiInputTextFlags_EnterReturnsTrue
    );

    ImGui::PopItemWidth();

    if (enterPressed) {
        std::string cmd(inputBuf);

        if (!cmd.empty()) {
            Log("> " + cmd, LogLevel::Info);
            ExecuteCommand(cmd);
        }

        inputBuf[0] = '\0';
    }

    ImGui::End();
}

void Console::Shutdown() {

}

void Console::Log(const std::string& message, LogLevel level) {
    messages.push_back({
        message,
        level,
        std::chrono::system_clock::now()
    });

    if (messages.size() > 1000)
        messages.erase(messages.begin());
}

void Console::ExecuteCommand(std::string command) {

}

void Console::SetVisible(bool visible) {
    this->visible = visible;
}