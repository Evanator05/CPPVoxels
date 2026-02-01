#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef struct _cArenaArray {
    size_t elem_size;
    size_t capacity;
    size_t count;
    float grow_factor;
    uint8_t *data;
} cArenaArray;

typedef struct _cArena {
    cArenaArray data;
    cArenaArray free;
    cArenaArray dirty;
} cArena;

typedef struct _cArenaSpan {
    size_t start;
    size_t size;
} cArenaSpan;

typedef cArenaSpan cArenaAllocation;

// array
void cArena_array_init(cArenaArray *array, size_t elem_size, size_t capacity, float grow_factor);
void cArena_array_destroy(cArenaArray *array);

bool cArena_array_resize(cArenaArray *array, size_t capacity);

uint8_t *cArena_array_at(cArenaArray *array, size_t index);
uint8_t *cArena_array_slot(cArenaArray *array, size_t index);

bool cArena_array_grow(cArenaArray *array);

bool cArena_array_add(cArenaArray *array, const uint8_t *element);
bool cArena_array_insert(cArenaArray *array, const uint8_t *element, size_t index);
void cArena_array_set(cArenaArray *array, const uint8_t *element, size_t index);
void cArena_array_place(cArenaArray *array, const uint8_t *element, size_t index);
void cArena_array_remove(cArenaArray *array, size_t index);

void cArena_array_move(cArenaArray *array, size_t from, size_t to);
void cArena_array_swap(cArenaArray *array, size_t index1, size_t index2);

void cArena_array_clear(cArenaArray *array);

// cArena
void cArena_init(cArena *cArena, size_t elem_size, size_t capacity);
void cArena_destroy(cArena *cArena);

bool cArena_resize(cArena *cArena, size_t capacity);
cArenaAllocation cArena_allocate(cArena *cArena, size_t count);
void cArena_free(cArena *cArena, cArenaAllocation allocation);

void cArena_set(cArena *cArena, const uint8_t *element, size_t index);
void cArena_clean(cArena *cArena);

void cArena_dirty_all(cArena *cArena);

#ifdef __cplusplus
}
#endif