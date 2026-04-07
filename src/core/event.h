#pragma once

#include <stdlib.h>
#include <vector>
#include <functional>
#include <algorithm>

template<typename... Args>
class Event {
    public:
        using Func = std::function<void(Args...)>;
        size_t Bind(Func func, bool single_use = false) {
            Binding binding{
                .id = nextId++,
                .func = func,
                .single_use = single_use
            };
            handlers.emplace_back(binding);
            return binding.id;
        }
        void Unbind(size_t handler_id) {
            handlers.erase(
                std::remove_if(handlers.begin(), handlers.end(),
                    [&](const Binding& b) { return b.id == handler_id; }),
                handlers.end()
            );
        }
        void Emit(Args... args) {
            for (Binding &binding : handlers) {
                binding.func(args...);
                if (binding.single_use) Unbind(binding.id);
            }
        }
    private:
        struct Binding {
            size_t id;
            Func func;
            bool single_use;
        };
        std::vector<Binding> handlers;
        size_t nextId = 0;
};

