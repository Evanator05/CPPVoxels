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

void cArena_init(cArena *cArena, size_t elem_size, size_t capacity) {
    cArena_array_init(&cArena->data, elem_size, capacity, 2);
    cArena_array_init(&cArena->free, sizeof(cArenaSpan), 10, 2);
    cArena_array_init(&cArena->dirty, sizeof(cArenaSpan), 10, 2);
}

void cArena_destroy(cArena *cArena) {
    cArena_array_destroy(&cArena->data);
    cArena_array_destroy(&cArena->free);
    cArena_array_destroy(&cArena->dirty);
}

bool cArena_resize(cArena *cArena, size_t capacity) {
    return cArena_array_resize(&cArena->data, capacity);
}

cArenaAllocation cArena_allocate(cArena *cArena, size_t size) {
    cArenaAllocation allocation = {
        .start = SIZE_MAX,
        .size  = size
    };

    // First-fit from free list
    for (size_t i = 0; i < cArena->free.count; i++) {
        cArenaAllocation *empty =
            (cArenaAllocation*)cArena_array_slot_unsafe(&cArena->free, i);

        if (empty->size < size)
            continue;

        allocation.start = empty->start;

        empty->start += size;
        empty->size  -= size;

        if (empty->size == 0)
            cArena_array_remove(&cArena->free, i);

        return allocation;
    }

    // Try to grow using tail free span
    size_t grow_by = size;
    if (cArena->free.count > 0) {
        cArenaAllocation *last =
            (cArenaAllocation*)cArena_array_at(&cArena->free, cArena->free.count - 1);

        if (last->start + last->size == cArena->data.capacity) {
            allocation.start = last->start;
            grow_by -= last->size;
            cArena_array_remove(&cArena->free, cArena->free.count - 1);
        }
    }

    // Otherwise grow from end
    if (allocation.start == SIZE_MAX)
        allocation.start = cArena->data.capacity;

    if (!cArena_array_resize(&cArena->data,
            cArena->data.capacity + grow_by)) {
        allocation.start = SIZE_MAX; // signal failure
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
    if (index >= cArena->data.capacity)
        return;

    cArena_array_place(&cArena->data, element, index);

    if (index >= cArena->data.count)
        cArena->data.count = index + 1;

    cArenaSpan newSpan = { index, 1 };
    cArenaArray *dirty = &cArena->dirty;

    if (dirty->count == 0) {
        cArena_array_add(dirty, (const uint8_t *)&newSpan);
        return;
    }

    size_t i = 0;

    // Find insertion point
    for (; i < dirty->count; ++i) {
        cArenaSpan *span =
            (cArenaSpan *)cArena_array_slot_unsafe(dirty, i);

        if (newSpan.start < span->start)
            break;

        size_t span_end = span->start + span->size;

        // Already covered
        if (newSpan.start >= span->start &&
            newSpan.start < span_end)
            return;

        // Merge right
        if (newSpan.start == span_end) {
            span->size += 1;

            // Merge with next span
            if (i + 1 < dirty->count) {
                cArenaSpan *next =
                    (cArenaSpan *)cArena_array_slot_unsafe(dirty, i + 1);

                if (span->start + span->size == next->start) {
                    span->size += next->size;
                    cArena_array_remove(dirty, i + 1);
                }
            }
            return;
        }
    }

    // Insert new span
    cArena_array_insert(dirty,
        (const uint8_t *)&newSpan,
        i);

    // Merge left if adjacent
    if (i > 0) {
        cArenaSpan *left =
            (cArenaSpan *)cArena_array_slot_unsafe(dirty, i - 1);
        cArenaSpan *curr =
            (cArenaSpan *)cArena_array_slot_unsafe(dirty, i);

        if (left->start + left->size == curr->start) {
            left->size += curr->size;
            cArena_array_remove(dirty, i);
        }
    }
}

void cArena_clean(cArena *cArena) {
    cArena_array_clear(&cArena->dirty);
}

void cArena_dirty_all(cArena *cArena) {
    cArena_array_clear(&cArena->dirty);

    if (cArena->data.capacity == 0)
        return;

    cArenaSpan dirty;
    dirty.start = 0;
    dirty.size  = cArena->data.capacity;

    cArena_array_add(
        &cArena->dirty,
        (const uint8_t *)&dirty
    );
}