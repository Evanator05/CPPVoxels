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
        
        template<typename... Args>
        void CreateCommand(const std::string& command, void(*func)(Args...))
        {
            commands[command] = [func](const std::vector<std::string>& args)
            {
                if (args.size() != sizeof...(Args))
                    throw std::runtime_error("Wrong number of arguments");

                CallFunction(func, args, std::index_sequence_for<Args...>{});
            };
        }
        void DeleteCommand(std::string command);
        void ExecuteCommand(std::string command);

        void SetVisible(bool visible);

    private:
        struct LogEntry {
            std::string message;
            LogLevel level;
            std::chrono::system_clock::time_point time;
        };
        template<typename T>
        static T Convert(const std::string &s);

        template<typename... Args, size_t... I>
        static void CallFunction(
            void(*func)(Args...),
            const std::vector<std::string>& args,
            std::index_sequence<I...>)
        {
            func(Convert<Args>(args[I])...);
        }
        std::vector<LogEntry> messages;

        std::unordered_map<std::string, std::function<void(const std::vector<std::string>&)>> commands;

        bool visible = true;

        void help();
};