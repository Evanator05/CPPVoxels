#include "arena.h"
#include <malloc.h>
#include <memory.h>
#include <string.h>
// array

void cArena_array_init(cArenaArray *array, size_t elem_size, size_t capacity, float grow_factor) {
    *array = (cArenaArray){0};
    array->data = NULL;
    if (elem_size == 0 || capacity == 0 || grow_factor <= 1.0f)
        return;

    array->data = malloc(elem_size*capacity);
    if (!array->data) return;

    array->capacity = capacity;
    array->elem_size = elem_size;
    array->grow_factor = grow_factor;
}

void cArena_array_destroy(cArenaArray *array) {
    if (array->data) free(array->data);
    *array = (cArenaArray){0};
}

bool cArena_array_resize(cArenaArray *array, size_t capacity) {
    uint8_t *new_data = realloc(array->data, capacity * array->elem_size);
    if (!new_data) return false;
    array->capacity = capacity;
    array->data = new_data;
    if (array->count > array->capacity) array->count = capacity;
    return true;
}

uint8_t *cArena_array_at(cArenaArray *array, size_t index) {
    if (index >= array->count) return NULL;
    return array->data + index * array->elem_size;
}

uint8_t *cArena_array_slot(cArenaArray *array, size_t index) {
    if (index >= array->capacity) return NULL;
    return array->data + index * array->elem_size;
}

static inline uint8_t *cArena_array_slot_unsafe(const cArenaArray *a, size_t i) {
    return a->data + i * a->elem_size;
}

bool cArena_array_grow(cArenaArray *array) {
    size_t new_cap = (size_t)(array->capacity * array->grow_factor);
    if (new_cap <= array->capacity)
        new_cap = array->capacity + 1;
    return cArena_array_resize(array, new_cap);
}

bool cArena_array_add(cArenaArray *array, const uint8_t *element) {
    size_t index = array->count;
    if (index >= array->capacity) // if the new element falls outside of the array grow its capacity
        if (!cArena_array_grow(array))
            return false;
    cArena_array_place(array, element, index);
    ++array->count;
    return true;
}

bool cArena_array_insert(cArenaArray *array, const uint8_t *element, size_t index) {
    if (index > array->count) return false;

    if (array->count == array->capacity)
        if (!cArena_array_grow(array))
            return false;

    uint8_t *base = array->data;
    size_t es = array->elem_size;

    memmove(
        base + (index + 1) * es,
        base + index * es,
        (array->count - index) * es
    );

    memcpy(base + index * es, element, es);
    ++array->count;
    return true;
}

void cArena_array_set(cArenaArray *array, const uint8_t *element, size_t index) {
    if (index >= array->count) return;
    memcpy(cArena_array_at(array, index), element, array->elem_size);
}

void cArena_array_place(cArenaArray *array, const uint8_t *element, size_t index) {
    if (index >= array->capacity) return;
    memcpy(cArena_array_slot_unsafe(array, index), element, array->elem_size);
}

void cArena_array_remove(cArenaArray *array, size_t index) {
    if (index >= array->count) return;

    uint8_t *base = array->data;
    size_t es = array->elem_size;

    memmove(
        base + index * es,
        base + (index + 1) * es,
        (array->count - index - 1) * es
    );

    --array->count;
}

void cArena_array_move(cArenaArray *array, size_t from, size_t to) {
    memcpy(cArena_array_slot_unsafe(array, to), cArena_array_slot_unsafe(array, from), array->elem_size);
}

void cArena_array_swap(cArenaArray *array, size_t index1, size_t index2) {
    if (index1 == index2) return;
    
    uint8_t *a = cArena_array_slot_unsafe(array, index1);
    uint8_t *b = cArena_array_slot_unsafe(array, index2);
    if (!a || !b) return;

    uint8_t temp[array->elem_size];

    memcpy(temp, a, array->elem_size);
    memcpy(a, b, array->elem_size);
    memcpy(b, temp, array->elem_size);
}

void cArena_array_clear(cArenaArray *array) {
    array->count = 0;
}

// cArena

static inline size_t calc_dirty_array_size(size_t capacity, size_t dirty_size) {
    if (capacity == 0) return 0;
    size_t dirty_units = (capacity + dirty_size - 1) / dirty_size;
    return (dirty_units + 63) >> 6;
}

void cArena_init(cArena *cArena, size_t elem_size, size_t capacity, size_t dirty_size) {
    cArena_array_init(&cArena->data, elem_size, capacity, 2);
    cArena_array_init(&cArena->free, sizeof(cArenaSpan), 10, 2);
    cArena_array_init(&cArena->dirty, sizeof(uint64_t), calc_dirty_array_size(capacity, dirty_size), 2);
    cArena->dirty_size = dirty_size;
}

void cArena_destroy(cArena *cArena) {
    cArena_array_destroy(&cArena->data);
    cArena_array_destroy(&cArena->free);
    cArena_array_destroy(&cArena->dirty);
}

bool cArena_resize(cArena *cArena, size_t capacity) {
    bool worked = true;
    worked &= cArena_array_resize(&cArena->data, capacity);
    worked &= cArena_array_resize(&cArena->dirty, calc_dirty_array_size(capacity, cArena->dirty_size));
    return worked;
}

cArenaAllocation cArena_allocate(cArena *cArena, size_t size) {
    cArenaAllocation allocation = { .start = SIZE_MAX, .size = size };

    size_t free_count = cArena->free.count;
    cArenaArray *free_list = &cArena->free;

    // FAST first-fit: scan free list
    for (size_t i = 0; i < free_count; ++i) {
        cArenaAllocation *span = (cArenaAllocation*)cArena_array_slot_unsafe(free_list, i);
        if (span->size < size) continue;

        allocation.start = span->start;

        span->start += size;
        span->size -= size;

        if (span->size == 0) {
            // remove without memmove: swap with last
            if (i != free_count - 1) {
                memcpy(span, cArena_array_slot_unsafe(free_list, free_count - 1), sizeof(cArenaAllocation));
            }
            free_list->count--;
        }

        return allocation;
    }

    // If free list empty or no fit, grow arena
    allocation.start = cArena->data.capacity;
    if (!cArena_array_resize(&cArena->data, cArena->data.capacity + size)) {
        allocation.start = SIZE_MAX; // fail
    }

    return allocation;
}

void cArena_free(cArena *cArena, cArenaSpan allocation) {
    // Try to merge with existing spans
    for (size_t i = 0; i < cArena->free.count; ++i) {
        cArenaSpan *span = (cArenaSpan*)cArena_array_slot_unsafe(&cArena->free, i);

        // Merge left
        if (span->start + span->size == allocation.start) {
            span->size += allocation.size;

            // Check for right merge
            if (i + 1 < cArena->free.count) {
                cArenaSpan *right = (cArenaSpan*)cArena_array_slot_unsafe(&cArena->free, i + 1);
                if (span->start + span->size == right->start) {
                    span->size += right->size;
                    cArena_array_remove(&cArena->free, i + 1);
                }
            }
            return;
        }

        // Merge right
        if (allocation.start + allocation.size == span->start) {
            span->start = allocation.start;
            span->size += allocation.size;
            return;
        }

        // Insert before
        if (allocation.start < span->start) {
            cArena_array_insert(&cArena->free, (uint8_t*)&allocation, i);
            return;
        }
    }

    // Insert at end
    cArena_array_insert(&cArena->free, (uint8_t*)&allocation, cArena->free.count);
}

void cArena_set(cArena *cArena, const uint8_t *element, size_t index) {
    if (!cArena || index >= cArena->data.capacity || cArena->dirty_size == 0)
        return;

    cArena_array_place(&cArena->data, element, index);

    // Figure out which bit corresponds to this element
    size_t dirty_bit = index / cArena->dirty_size;

    // Figure out which 64-bit word and bit within the word
    size_t word_idx = dirty_bit / 64;
    size_t bit_idx  = dirty_bit % 64;

    uint64_t *bits = (uint64_t *)cArena->dirty.data;
    bits[word_idx] |= (1ULL << bit_idx); // mark as dirty
}

cArenaSpan *cArena_get_dirty_spans(cArena *cArena, size_t *out_count) {
    if (!cArena || cArena->dirty.capacity == 0 || cArena->dirty_size == 0) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    uint64_t *bits = (uint64_t *)cArena->dirty.data;
    size_t dirty_size = cArena->dirty_size;
    size_t max_elems = cArena->data.capacity;
    size_t total_dirty_units = (max_elems + dirty_size - 1) / dirty_size;

    // Worst case: each dirty unit is its own span
    cArenaSpan *spans = malloc(total_dirty_units * sizeof(cArenaSpan));
    if (!spans) { if (out_count) *out_count = 0; return NULL; }

    size_t span_idx = 0;
    size_t i = 0; // current dirty unit

    while (i < total_dirty_units) {
        size_t word_idx = i / 64;
        size_t bit_idx  = i % 64;
        uint64_t word = bits[word_idx];

        // Skip clean units
        if (!(word & (1ULL << bit_idx))) {
            i++;
            continue;
        }

        // Start of a dirty span
        size_t span_start_unit = i;
        while (i < total_dirty_units) {
            word_idx = i / 64;
            bit_idx  = i % 64;
            word = bits[word_idx];
            if (!(word & (1ULL << bit_idx))) break; // end of run
            i++;
        }

        size_t elem_start = span_start_unit * dirty_size;
        size_t elem_len   = (i - span_start_unit) * dirty_size;
        if (elem_start + elem_len > max_elems)
            elem_len = max_elems - elem_start;

        spans[span_idx++] = (cArenaSpan){ elem_start, elem_len };
    }

    if (out_count) *out_count = span_idx;
    return spans;
}


void cArena_clean(cArena *cArena) {
    memset(cArena->dirty.data, 0, cArena->dirty.elem_size*cArena->dirty.capacity);
}

void cArena_dirty_all(cArena *cArena) {
    if (!cArena || !cArena->dirty.data || cArena->dirty_size == 0)
        return;

    size_t max_elems = cArena->data.capacity;  // ALWAYS use capacity
    if (max_elems == 0)
        return;

    size_t total_bits = (max_elems + cArena->dirty_size - 1) / cArena->dirty_size;
    uint64_t *bits = (uint64_t *)cArena->dirty.data;

    // Clear all words first
    size_t words = cArena->dirty.capacity;
    for (size_t i = 0; i < words; ++i)
        bits[i] = 0;

    // Set exactly the bits needed to cover the arena capacity
    for (size_t i = 0; i < total_bits; ++i)
        bits[i / 64] |= (1ULL << (i % 64));
}