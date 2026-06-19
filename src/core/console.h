#pragma once

#include "engine.h"

#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <functional>
#include <tuple>
#include <stdexcept>
#include <sstream>
#include <iomanip>

class Console : public EngineModule {
public:
    using EngineModule::EngineModule;

    enum class LogLevel {
        Info,
        Warning,
        Error
    };

    void Init() override;
    void Process() override;
    void Shutdown() override;

    void Log(const std::string& message, LogLevel level);

    void ExecuteCommand(const std::string& command);
    void DeleteCommand(const std::string& command);

    void SetVisible(bool visible);

    template<typename F>
    void CreateCommand(const std::string& name, F&& func) {
        using Fn = std::decay_t<F>;
        using traits = function_traits<Fn>;
        using args_tuple = typename traits::args_tuple;

        constexpr size_t N = std::tuple_size_v<args_tuple>;

        commands[name] =
            [func = std::forward<F>(func)]
            (const std::vector<std::string>& args) mutable
            {
                if (args.size() != N)
                    throw std::runtime_error("Wrong number of arguments");

                invoke_from_strings<Fn, args_tuple>(
                    func,
                    args,
                    std::make_index_sequence<N>{});
            };
    }

private:
    struct LogEntry {
        std::string message;
        LogLevel level;
        std::chrono::system_clock::time_point time;
    };

    std::vector<LogEntry> messages;

    std::unordered_map<
        std::string,
        std::function<void(const std::vector<std::string>&)>
    > commands;

    bool visible = true;

private:
    template<typename T>
    struct function_traits;

    template<typename R, typename... Args>
    struct function_traits<R(*)(Args...)> {
        using args_tuple = std::tuple<Args...>;
    };

    template<typename R, typename... Args>
    struct function_traits<std::function<R(Args...)>> {
        using args_tuple = std::tuple<Args...>;
    };

    template<typename C, typename R, typename... Args>
    struct function_traits<R(C::*)(Args...) const> {
        using args_tuple = std::tuple<Args...>;
    };

    template<typename T>
    struct function_traits : function_traits<decltype(&T::operator())> {};

    template<typename T>
    static T Convert(const std::string& s);

    template<typename Func, typename Tuple, size_t... I>
    static void invoke_from_strings(
        Func& func,
        const std::vector<std::string>& args,
        std::index_sequence<I...>) {
        func(Convert<std::tuple_element_t<I, Tuple>>(args[I])...);
    }

    void help();
};