#pragma once

#include "arena.h"

template <typename T>
class Arena {
    public:
        Arena(size_t capacity);

        ~Arena();

        bool resize(size_t capacity);
        cArenaAllocation allocate(size_t count);
        void free(cArenaAllocation allocation);

        void set(const T *element, size_t index);

        cArenaArray *dirty();
        void dirty_all();
        void clean();

        T* data();
        size_t size();

        T* operator[] (size_t i) {
            return data()+i;
        }

    private:
        ::cArena c_Arena;
};

template <typename T>
Arena<T>::Arena(size_t capacity) {
    cArena_init(&c_Arena, sizeof(T), capacity);
}

template <typename T>
Arena<T>::~Arena() {
    cArena_destroy(&c_Arena);
}

template <typename T>
bool Arena<T>::resize(size_t capacity) {
    return cArena_resize(&c_Arena, capacity);
}

template <typename T>
cArenaAllocation Arena<T>::allocate(size_t count) {
    return cArena_allocate(&c_Arena, count);
}

template <typename T>
void Arena<T>::free(cArenaAllocation allocation) {
    cArena_free(&c_Arena, allocation);
}

template <typename T>
void Arena<T>::set(const T *element, size_t index) {
    cArena_set(&c_Arena, (const uint8_t *)element, index);
}

template <typename T>
cArenaArray *Arena<T>::dirty() {
    return &c_Arena.dirty;
}

template <typename T>
void Arena<T>::dirty_all() {
    cArena_dirty_all(&c_Arena);
}

template <typename T>
void Arena<T>::clean() {
    cArena_clean(&c_Arena);
}

template <typename T>
T* Arena<T>::data() {
    return (T*)c_Arena.data.data;
}

template <typename T>
size_t Arena<T>::size() {
    return c_Arena.data.capacity;
}