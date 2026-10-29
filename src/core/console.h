#pragma once

#include "engine.h"

#include <array>
#include <chrono>
#include <functional>
#include <meta>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

class Console : public EngineModule {
public:
    using EngineModule::EngineModule;

    enum class LogLevel {
        Info,
        Warning,
        Error
    };

    struct Message {
        std::string message;
        LogLevel level;
        std::chrono::system_clock::time_point time;
    };

    void Init() override;
    void Process() override;
    void Shutdown() override;

    void Log(const std::string& message, LogLevel level = LogLevel::Info);
    void DeleteCommand(const std::string& name);
    void ExecuteCommand(const std::string& command);
    void SetVisible(bool visible);

    template<typename F>
    void CreateCommand(const std::string& name, F&& callback) {
        using Callable = std::decay_t<F>;
        using Arguments = typename FunctionTraits<Callable>::Arguments;

        Command command;
        command.signature = GetSignature<Callable>();
        command.callback = [fn = std::forward<F>(callback)](
            const std::vector<std::string>& args
        ) mutable {
            DispatchCommand<Arguments>(fn, args);
        };

        commands.insert_or_assign(name, std::move(command));
    }

private:
    static constexpr std::size_t MaxMessages = 2048;
    static constexpr std::size_t InputCapacity = 512;
    static constexpr int MaxSuggestions = 8;

    struct Command {
        std::function<void(const std::vector<std::string>&)> callback;
        std::string signature;
    };

    struct PerformanceHistory {
        static constexpr int Capacity = 240;

        std::array<float, Capacity> frameMs{};
        int next = 0;
        int count = 0;
        float elapsed = 0.0f;
        int elapsedFrames = 0;
        float displayedFps = 0.0f;
        float displayedFrameMs = 0.0f;

        void AddFrame(float seconds);
        float Sample(int chronologicalIndex) const;
        static float PlotGetter(void* data, int index);
    };

    std::vector<Message> messages;
    std::unordered_map<std::string, Command> commands;
    PerformanceHistory performance;

    std::array<char, InputCapacity> inputBuffer{};
    bool visible = false;
    bool refocusInput = false;
    bool autoScroll = true;
    bool showPerformance = true;

    // Rendering
    void DrawConsole();
    void DrawMessages(float height);
    void DrawCommandInput();
    void DrawSuggestions();
    void DrawPerformancePanel() const;

    // Command handling and completion
    static std::vector<std::string> TokenizeCommand(std::string_view input);
    static std::string FormatTime(const std::chrono::system_clock::time_point& time);
    std::vector<std::string> FindCommands(std::string_view prefix) const;
    std::string CompleteCommandName(std::string_view prefix) const;

    // Command argument types
    template<typename T>
    struct FunctionTraits : FunctionTraits<decltype(&T::operator())> {};

    template<typename R, typename... Args>
    struct FunctionTraits<R(Args...)> {
        using Arguments = std::tuple<Args...>;
    };

    template<typename R, typename... Args>
    struct FunctionTraits<R(*)(Args...)> : FunctionTraits<R(Args...)> {};

    template<typename R, typename... Args>
    struct FunctionTraits<R(*)(Args...) noexcept> : FunctionTraits<R(Args...)> {};

    template<typename C, typename R, typename... Args>
    struct FunctionTraits<R(C::*)(Args...)> : FunctionTraits<R(Args...)> {};

    template<typename C, typename R, typename... Args>
    struct FunctionTraits<R(C::*)(Args...) const> : FunctionTraits<R(Args...)> {};

    template<typename C, typename R, typename... Args>
    struct FunctionTraits<R(C::*)(Args...) noexcept> : FunctionTraits<R(Args...)> {};

    template<typename C, typename R, typename... Args>
    struct FunctionTraits<R(C::*)(Args...) const noexcept> : FunctionTraits<R(Args...)> {};

    // C++26 reflection (source parameter names included)
    static consteval std::string_view FriendlyTypeName(std::meta::info reflectedType) {
        const auto type = std::meta::dealias(std::meta::remove_cvref(reflectedType));

        if (type == std::meta::dealias(^^std::string))
            return "string";
        if (type == std::meta::dealias(^^std::string_view))
            return "string_view";
        if (type == std::meta::dealias(^^std::vector<std::string>))
            return "string[]";
        if (type == ^^bool)   return "bool";
        if (type == ^^int)    return "int";
        if (type == ^^float)  return "float";
        if (type == ^^double) return "double";

        return std::meta::display_string_of(type);
    }

    template<std::meta::info Function>
    static consteval std::string_view BuildSignature() {
        std::string signature;

        for (auto parameter : std::meta::parameters_of(Function)) {
            signature += " <";

            if (std::meta::is_type(parameter)) {
                signature += FriendlyTypeName(parameter);
            } else {
                if (std::meta::has_identifier(parameter)) {
                    signature += std::meta::identifier_of(parameter);
                    signature += ": ";
                }
                signature += FriendlyTypeName(std::meta::type_of(parameter));
            }

            signature += ">";
        }

        return std::define_static_string(signature);
    }

    template<typename F>
    static consteval std::string_view GetSignature() {
        using T = std::remove_cvref_t<F>;

        if constexpr (std::is_class_v<T>) {
            return BuildSignature<^^T::operator()>();
        } else if constexpr (std::is_pointer_v<T>) {
            using Function = std::remove_pointer_t<T>;
            return BuildSignature<^^Function>();
        } else {
            return BuildSignature<^^T>();
        }
    }

    template<typename T>
    static std::remove_cvref_t<T> Convert(const std::string& value) {
        using Value = std::remove_cvref_t<T>;

        if constexpr (std::is_same_v<Value, int>) {
            return std::stoi(value);
        } else if constexpr (std::is_same_v<Value, float>) {
            return std::stof(value);
        } else if constexpr (std::is_same_v<Value, double>) {
            return std::stod(value);
        } else if constexpr (std::is_same_v<Value, bool>) {
            if (value == "true" || value == "1" || value == "on") return true;
            if (value == "false" || value == "0" || value == "off") return false;
            throw std::invalid_argument("Invalid boolean: " + value);
        } else if constexpr (std::is_same_v<Value, std::string>) {
            return value;
        } else if constexpr (std::is_same_v<Value, std::string_view>) {
            return value;
        } else {
            static_assert(!std::is_same_v<Value, Value>,
                "Unsupported console argument type");
        }
    }

    template<typename Arguments, typename F, std::size_t... I>
    static void InvokeParsedCommand(F& callback, const std::vector<std::string>& args, std::index_sequence<I...>) {
        if (args.size() != sizeof...(I)) {
            throw std::runtime_error(
                "Expected " + std::to_string(sizeof...(I)) +
                " arguments, got " + std::to_string(args.size())
            );
        }
        // Own values for the duration of the call, including reference parameters.
        std::tuple<std::remove_cvref_t<std::tuple_element_t<I, Arguments>>...> values{
            Convert<std::tuple_element_t<I, Arguments>>(args[I])...
        };
        std::apply(callback, values);
    }

    template<typename Arguments, typename F>
    static void DispatchCommand(F& callback, const std::vector<std::string>& args) {
        constexpr std::size_t Count = std::tuple_size_v<Arguments>;

        if constexpr (Count == 1) {
            using First = std::remove_cvref_t<std::tuple_element_t<0, Arguments>>;

            if constexpr (std::is_same_v<First, std::vector<std::string>>) {
                std::invoke(callback, args);
            } else {
                InvokeParsedCommand<Arguments>(callback, args, std::make_index_sequence<Count>{});
            }
        } else {
            InvokeParsedCommand<Arguments>(callback, args, std::make_index_sequence<Count>{});
        }
    }
};
