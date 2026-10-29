#include "console.h"
#include "gui.h"
#include "input.h"
#include "deltatime.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------

void Console::Init() {
    auto& input = GetModule<Input>();
    input.CreateAction("toggleconsole");
    input.CreateBinding("toggleconsole", SDLK_GRAVE);

    CreateCommand("getfps", [this]() {
        const float dt = GetModule<DeltaTime>().Get();
        if (dt > 0.0f)
            Log(std::to_string(1.0f / dt) + " FPS");
    });

    CreateCommand("clear", [this]() {
        messages.clear();
    });

    CreateCommand("help", [this]() {
        const auto names = FindCommands("");
        Log("Available commands (" + std::to_string(names.size()) + "):");

        for (const auto& name : names)
            Log("  " + name + commands.at(name).signature);
    });
}

void Console::Process() {
    performance.AddFrame(GetModule<DeltaTime>().Get());

    if (GetModule<Input>().IsPressed("toggleconsole"))
        SetVisible(!visible);

    if (visible)
        DrawConsole();
}

void Console::Shutdown() {}

// -----------------------------------------------------------------------------
// Public command API
// -----------------------------------------------------------------------------

void Console::Log(const std::string& message, LogLevel level) {
    messages.push_back({message, level, std::chrono::system_clock::now()});

    if (messages.size() > MaxMessages)
        messages.erase(messages.begin());
}

void Console::DeleteCommand(const std::string& name) {
    commands.erase(name);
}

void Console::ExecuteCommand(const std::string& command) {
    auto tokens = TokenizeCommand(command);
    if (tokens.empty())
        return;

    const auto it = commands.find(tokens.front());
    if (it == commands.end()) {
        Log("Unknown command: " + tokens.front(), LogLevel::Error);
        return;
    }

    std::vector<std::string> args(tokens.begin() + 1, tokens.end());

    try {
        it->second.callback(args);
    } catch (const std::exception& e) {
        Log(std::string("Command error: ") + e.what(), LogLevel::Error);
    }
}

void Console::SetVisible(bool value) {
    if (value && !visible)
        refocusInput = true;
    visible = value;
}

// -----------------------------------------------------------------------------
// Parsing and autocomplete
// -----------------------------------------------------------------------------

std::vector<std::string> Console::TokenizeCommand(std::string_view input) {
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false;

    for (std::size_t i = 0; i < input.size(); ++i) {
        const char c = input[i];

        if (c == '\\' && i + 1 < input.size()) {
            const char next = input[i + 1];
            if (next == '"' || next == '\\') {
                current += next;
                ++i;
                continue;
            }
        }

        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (!inQuotes && std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                tokens.push_back(std::move(current));
                current.clear();
            }
        } else {
            current += c;
        }
    }

    if (!current.empty())
        tokens.push_back(std::move(current));

    return tokens;
}

std::vector<std::string> Console::FindCommands(std::string_view prefix) const {
    std::vector<std::string> matches;

    for (const auto& [name, command] : commands) {
        if (name.size() >= prefix.size() &&
            name.compare(0, prefix.size(), prefix) == 0) {
            matches.push_back(name);
        }
    }

    std::sort(matches.begin(), matches.end());
    return matches;
}

std::string Console::CompleteCommandName(std::string_view prefix) const {
    if (prefix.empty() || prefix.find_first_of(" \t\r\n") != std::string_view::npos)
        return {};

    const auto matches = FindCommands(prefix);
    if (matches.empty())
        return {};

    std::string completion = matches.front();

    for (std::size_t i = 1; i < matches.size(); ++i) {
        std::size_t common = 0;
        while (common < completion.size() &&
               common < matches[i].size() &&
               completion[common] == matches[i][common]) {
            ++common;
        }
        completion.resize(common);
    }

    if (matches.size() == 1)
        completion += ' ';

    return completion;
}

std::string Console::FormatTime(
    const std::chrono::system_clock::time_point& time
) {
    const std::time_t t = std::chrono::system_clock::to_time_t(time);
    std::tm tm{};

#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    std::ostringstream stream;
    stream << std::put_time(&tm, "%H:%M:%S");
    return stream.str();
}

// -----------------------------------------------------------------------------
// Performance history
// -----------------------------------------------------------------------------

void Console::PerformanceHistory::AddFrame(float seconds) {
    if (!std::isfinite(seconds) || seconds <= 0.0f || seconds > 1.0f)
        return;

    frameMs[next] = seconds * 1000.0f;
    next = (next + 1) % Capacity;
    count = std::min(count + 1, Capacity);

    elapsed += seconds;
    ++elapsedFrames;

    if (elapsed >= 0.25f) {
        displayedFps = static_cast<float>(elapsedFrames) / elapsed;
        displayedFrameMs = (elapsed / elapsedFrames) * 1000.0f;
        elapsed = 0.0f;
        elapsedFrames = 0;
    }
}

float Console::PerformanceHistory::Sample(int chronologicalIndex) const {
    const int oldest = (next - count + Capacity) % Capacity;
    return frameMs[(oldest + chronologicalIndex) % Capacity];
}

float Console::PerformanceHistory::PlotGetter(void* data, int index) {
    return static_cast<const PerformanceHistory*>(data)->Sample(index);
}

void Console::DrawConsole() {
    ImGui::SetNextWindowSize(ImVec2(800.0f, 520.0f), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Console", &visible)) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Clear"))
        messages.clear();

    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &autoScroll);
    ImGui::SameLine();
    ImGui::Checkbox("Show Performance", &showPerformance);

    const float commandHeight =
        ImGui::GetFrameHeightWithSpacing() +
        ImGui::GetStyle().ItemSpacing.y * 2.0f;

    const float panelHeight = showPerformance ? 225.0f : 0.0f;
    const float messageHeight = std::max(
        80.0f,
        ImGui::GetContentRegionAvail().y - commandHeight - panelHeight
    );

    ImGui::Separator();
    DrawMessages(messageHeight);
    ImGui::Separator();
    DrawCommandInput();
    ImGui::Separator();

    if (showPerformance)
        DrawPerformancePanel();

    ImGui::End();
}

void Console::DrawMessages(float height) {
    ImGui::BeginChild(
        "ScrollRegion",
        ImVec2(0.0f, height),
        true,
        ImGuiWindowFlags_HorizontalScrollbar
    );

    const bool wasAtBottom =
        ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 2.0f;

    if (ImGui::BeginTable(
        "ConsoleTable",
        3,
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_SizingStretchProp |
        ImGuiTableFlags_ScrollY
    )) {
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (const auto& entry : messages) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            const auto time = FormatTime(entry.time);
            ImGui::TextUnformatted(time.c_str());

            ImGui::TableSetColumnIndex(1);
            switch (entry.level) {
                case LogLevel::Info:    ImGui::TextUnformatted("INFO");  break;
                case LogLevel::Warning: ImGui::TextUnformatted("WARN");  break;
                case LogLevel::Error:   ImGui::TextUnformatted("ERROR"); break;
            }

            ImGui::TableSetColumnIndex(2);
            switch (entry.level) {
                case LogLevel::Warning:
                    ImGui::TextColored({1.0f, 0.8f, 0.2f, 1.0f}, "%s", entry.message.c_str());
                    break;
                case LogLevel::Error:
                    ImGui::TextColored({1.0f, 0.3f, 0.3f, 1.0f}, "%s", entry.message.c_str());
                    break;
                default:
                    ImGui::TextUnformatted(entry.message.c_str());
                    break;
            }
        }

        ImGui::EndTable();
    }

    if (autoScroll && wasAtBottom)
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
}

void Console::DrawCommandInput() {
    ImGui::TextUnformatted("Command:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);

    if (refocusInput) {
        ImGui::SetKeyboardFocusHere();
        refocusInput = false;
    }

    const auto callback = [](ImGuiInputTextCallbackData* data) -> int {
        if (data->EventFlag != ImGuiInputTextFlags_CallbackCompletion)
            return 0;

        const auto* console = static_cast<const Console*>(data->UserData);
        const auto completion = console->CompleteCommandName(
            std::string_view(data->Buf, static_cast<std::size_t>(data->BufTextLen))
        );

        if (!completion.empty()) {
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, completion.c_str());
        }

        return 0;
    };

    const bool submitted = ImGui::InputText(
        "##cmd",
        inputBuffer.data(),
        inputBuffer.size(),
        ImGuiInputTextFlags_EnterReturnsTrue |
        ImGuiInputTextFlags_CallbackCompletion,
        callback,
        this
    );

    if (submitted) {
        const std::string command(inputBuffer.data());
        inputBuffer[0] = '\0';
        refocusInput = true;

        if (!command.empty()) {
            Log("> " + command);
            ExecuteCommand(command);
        }
    } else {
        DrawSuggestions();
    }
}

void Console::DrawSuggestions() {
    const std::string_view typed(inputBuffer.data());
    if (typed.empty() || typed.find_first_of(" \t\r\n") != std::string_view::npos)
        return;

    const auto matches = FindCommands(typed);
    if (matches.empty())
        return;

    const ImVec2 inputMin = ImGui::GetItemRectMin();
    const ImVec2 inputMax = ImGui::GetItemRectMax();
    const ImVec2 nextCursor = ImGui::GetCursorScreenPos();
    const auto& style = ImGui::GetStyle();
    const int rows = std::min(MaxSuggestions, static_cast<int>(matches.size()));
    const float height =
        rows * ImGui::GetTextLineHeightWithSpacing() +
        style.WindowPadding.y * 2.0f + 4.0f;

    ImGui::SetCursorScreenPos(ImVec2(inputMin.x, inputMin.y - height - 4.0f));
    ImGui::SetNextWindowBgAlpha(0.98f);

    if (ImGui::BeginChild(
        "##ConsoleCommandSuggestions",
        ImVec2(inputMax.x - inputMin.x, height),
        ImGuiChildFlags_Borders,
        ImGuiWindowFlags_NoNavFocus
    )) {
        for (int i = 0; i < rows; ++i) {
            const auto& name = matches[i];
            if (ImGui::Selectable(name.c_str())) {
                std::snprintf(inputBuffer.data(), inputBuffer.size(), "%s ", name.c_str());
                refocusInput = true;
            }
        }
    }

    ImGui::EndChild();
    ImGui::SetCursorScreenPos(nextCursor);
}

void Console::DrawPerformancePanel() const {
    ImGui::Separator();
    ImGui::TextDisabled("PERFORMANCE");

    if (performance.count == 0) {
        ImGui::TextDisabled("Collecting frame data...");
        return;
    }

    float total = 0.0f;
    float minimum = std::numeric_limits<float>::max();
    float maximum = 0.0f;

    for (int i = 0; i < performance.count; ++i) {
        const float ms = performance.Sample(i);
        total += ms;
        minimum = std::min(minimum, ms);
        maximum = std::max(maximum, ms);
    }

    const float mean = total / performance.count;

    if (ImGui::BeginTable("##PerformanceMetrics", 4, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::TextDisabled("FPS");
        ImGui::Text("%.0f", performance.displayedFps);

        ImGui::TableNextColumn();
        ImGui::TextDisabled("Frame time");
        ImGui::Text("%.2f ms", performance.displayedFrameMs);

        ImGui::TableNextColumn();
        ImGui::TextDisabled("Best (recent)");
        ImGui::Text("%.2f ms", minimum);

        ImGui::TableNextColumn();
        ImGui::TextDisabled("Worst (recent)");
        ImGui::Text("%.2f ms", maximum);

        ImGui::EndTable();
    }

    ImGui::TextDisabled(
        "Frame time history (%d frames)  |  Mean %.2f ms",
        performance.count,
        mean
    );

    const float graphMax = std::max(20.0f, std::ceil(maximum / 5.0f) * 5.0f);

    ImGui::PlotLines(
        "##FrameTimeHistory",
        &PerformanceHistory::PlotGetter,
        const_cast<PerformanceHistory*>(&performance),
        performance.count,
        0,
        nullptr,
        0.0f,
        graphMax,
        ImVec2(-1.0f, 90.0f)
    );

    ImGui::TextDisabled("8.33 ms = 120 FPS   |   16.67 ms = 60 FPS");
}
