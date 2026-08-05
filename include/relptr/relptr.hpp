#pragma once
#include <vector>
#include <cinttypes>
#include <limits>
#include <concepts>
#include <type_traits>

#define RELPTR_TAG(name) struct name

template<typename Tag, typename T>
struct RelptrBase {
    using value_type = T;
    
    static inline T* base = nullptr;

    static void set_base(T* b) { base = b; }
    static T* get_base() { return base; }
};

template<typename Tag, typename T>
struct RelptrBaseVector : RelptrBase<Tag, T> {
    using value_type = T;
    static inline std::vector<T>* vec = nullptr;

    static void set_base(std::vector<T>& v) { vec = &v; }
    static T* get_base() { return vec ? vec->data() : nullptr; }
};

template<typename B>
concept RelptrProvider =
requires {
    typename B::value_type;
    { B::get_base() } -> std::same_as<typename B::value_type*>;
};

template<RelptrProvider BaseProvider, typename I = uint32_t>
struct Relptr {
public:
    using T = typename BaseProvider::value_type;

    I offset{};
    
    static constexpr I null_value =
        std::numeric_limits<I>::max();

    Relptr(I i) : offset(i) {}
    Relptr(T *ptr) {
        set_offset(ptr);
    }
    Relptr() : offset(null_value) {}

    T* resolve() const {
        if (offset == null_value) return nullptr;

        T* base = BaseProvider::get_base();
        return base + offset;
    }

    void set_offset(T *ptr) {
        if (!ptr) {
            offset = null_value;
            return;
        }

        auto base = BaseProvider::get_base();
        auto diff = ptr - base;

        if (diff < 0) {
            offset = null_value;
            return;
        }

        offset = static_cast<I>(diff);
    }
    
    T* operator->() const { return resolve(); }
    T& operator*() const { return *resolve(); }
    operator T*() const { return resolve(); }

    bool operator==(std::nullptr_t) const {
        return offset == null_value;
    }

    bool operator!=(std::nullptr_t) const {
        return offset != null_value;
    }

    bool operator==(const Relptr& other) const {
        return offset == other.offset;
    }

    bool operator!=(const Relptr& other) const {
        return offset != other.offset;
    }
};