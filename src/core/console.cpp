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

void Console::DeleteCommand(std::string command)
{
    commands.erase(command);
}

static std::vector<std::string> TokenizeCommand(const std::string& input)
{
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false;

    for (size_t i = 0; i < input.size(); i++)
    {
        char c = input[i];

        if (c == '\\' && i + 1 < input.size())
        {
            char next = input[i + 1];

            if (next == '"' || next == '\\')
            {
                current += next;
                i++;
                continue;
            }
        }

        if (c == '"')
        {
            inQuotes = !inQuotes;
            continue;
        }

        if (!inQuotes && std::isspace(static_cast<unsigned char>(c)))
        {
            if (!current.empty())
            {
                tokens.push_back(current);
                current.clear();
            }
        }
        else
        {
            current += c;
        }
    }

    if (!current.empty())
        tokens.push_back(current);

    return tokens;
}

void Console::ExecuteCommand(std::string command)
{
    std::vector<std::string> tokens = TokenizeCommand(command);

    if (tokens.empty())
        return;

    std::string name = tokens[0];

    auto it = commands.find(name);
    if (it == commands.end())
    {
        Log("Unknown command, use help for more info: " + name, LogLevel::Error);
        return;
    }

    std::vector<std::string> args(tokens.begin() + 1, tokens.end());

    try
    {
        it->second(args);
    }
    catch (const std::exception& e)
    {
        Log(std::string("Command error: ") + e.what(), LogLevel::Error);
    }
}

void Console::SetVisible(bool visible) {
    this->visible = visible;
}

void Console::help() {
    Log("test", LogLevel::Info);
    Log("test", LogLevel::Info);
    Log("test", LogLevel::Info);
    Log("test", LogLevel::Info);
    Log("test", LogLevel::Info);
}

template<>
int Console::Convert<int>(const std::string &s) {
    return std::stoi(s);
}

template<>
float Console::Convert<float>(const std::string &s) {
    return std::stof(s);
}

template<>
double Console::Convert<double>(const std::string &s) {
    return std::stod(s);
}

template<>
std::string Console::Convert<std::string>(const std::string &s) {
    return s;
}