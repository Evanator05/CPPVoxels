#pragma once

#include "engine.h"

#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <functional>

class Console : public EngineModule {
    public:
        enum class LogLevel {
            Info,
            Warning,
            Error
        };
        using EngineModule::EngineModule;
        void Init(void) override;
        void Process(void) override;
        void Shutdown(void) override;

        void Log(const std::string& message, LogLevel level);
        
        void CreateCommand(std::string command);
        void DeleteCommand(std::string command);
        void TokenizeCommand(std::string command);
        void ExecuteCommand(std::string command);

        void SetVisible(bool visible);

    private:
        struct LogEntry {
            std::string message;
            LogLevel level;
            std::chrono::system_clock::time_point time;
        };

        std::vector<LogEntry> messages;
        bool visible = true;
};