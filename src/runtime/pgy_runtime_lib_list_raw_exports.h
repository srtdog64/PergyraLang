#ifndef PGY_RUNTIME_LIB_LIST_RAW_EXPORTS_H
#define PGY_RUNTIME_LIB_LIST_RAW_EXPORTS_H

/* LLVM-linkable raw List<T> exports used by generic collection lowering. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    void   *data;
    size_t  count;
    size_t  capacity;
} PgyListRaw;

static bool
pgy_list_raw_shape_fits(size_t capacity, size_t elem_size)
{
    return capacity != 0 && elem_size != 0 && elem_size <= SIZE_MAX / capacity;
}

void
pgy_list_new_raw_export(void *list_ptr, int64_t elem_size)
{
    PgyListRaw *list = (PgyListRaw *)list_ptr;
    if (list == NULL) {
        pgy_runtime_warn_invalid_collection("list_new", "null list");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("list_new", "non-positive element size");
        return;
    }
    list->capacity = 16;
    if (!pgy_list_raw_shape_fits(list->capacity, (size_t)elem_size)) {
        list->capacity = 0;
        pgy_runtime_warn_invalid_collection("list_new", "allocation size overflow");
        return;
    }
    list->count = 0;
    list->data = calloc((size_t)list->capacity, (size_t)elem_size);
    if (list->data == NULL) {
        list->capacity = 0;
        pgy_runtime_warn_invalid_collection("list_new", "allocation failed");
    }
}

void
pgy_list_push_raw_export(void *list_ptr, void *value_ptr, int64_t elem_size)
{
    PgyListRaw *list = (PgyListRaw *)list_ptr;
    char *dst;
    if (list == NULL) {
        pgy_runtime_warn_invalid_collection("list_push", "null list");
        return;
    }
    if (value_ptr == NULL) {
        pgy_runtime_warn_invalid_collection("list_push", "null value");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("list_push", "non-positive element size");
        return;
    }
    if (list->data == NULL && list->capacity == 0) {
        pgy_runtime_warn_invalid_collection("list_push", "list is not initialized");
        return;
    }
    if (list->count >= list->capacity) {
        size_t new_capacity;
        void *grown;
        if (list->capacity == 0) {
            new_capacity = 16;
        } else {
            if (list->capacity > SIZE_MAX / 2) {
                pgy_runtime_warn_invalid_collection("list_push", "capacity overflow");
                return;
            }
            new_capacity = list->capacity * 2;
        }
        if (!pgy_list_raw_shape_fits(new_capacity, (size_t)elem_size)) {
            pgy_runtime_warn_invalid_collection("list_push", "allocation size overflow");
            return;
        }
        grown = realloc(list->data, new_capacity * (size_t)elem_size);
        if (grown == NULL) {
            pgy_runtime_warn_invalid_collection("list_push", "realloc failed");
            return;
        }
        list->data = grown;
        list->capacity = new_capacity;
    }
    dst = (char *)list->data + (list->count * (size_t)elem_size);
    memcpy(dst, value_ptr, (size_t)elem_size);
    list->count++;
}

void
pgy_list_get_raw_export(void *list_ptr, int32_t index, void *out_ptr, int64_t elem_size)
{
    PgyListRaw *list = (PgyListRaw *)list_ptr;
    if (out_ptr == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list get on null output");
    }
    if (elem_size <= 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list get with invalid element size");
    }
    memset(out_ptr, 0, (size_t)elem_size);
    if (list == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list get on null list");
    }
    if (index < 0 || (size_t)index >= list->count) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "list index out of bounds");
    }
    memcpy(out_ptr,
           (char *)list->data + ((size_t)index * (size_t)elem_size),
           (size_t)elem_size);
}

void
pgy_list_set_raw_export(void *list_ptr, int32_t index, void *value_ptr, int64_t elem_size)
{
    PgyListRaw *list = (PgyListRaw *)list_ptr;
    if (list == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list set on null list");
    }
    if (value_ptr == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list set with null value");
    }
    if (elem_size <= 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list set with invalid element size");
    }
    if (index < 0 || (size_t)index >= list->count) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "list set index out of bounds");
    }
    memcpy((char *)list->data + ((size_t)index * (size_t)elem_size),
           value_ptr, (size_t)elem_size);
}

int32_t
pgy_list_size_raw_export(void *list_ptr)
{
    PgyListRaw *list = (PgyListRaw *)list_ptr;
    if (list == NULL) {
        pgy_runtime_warn_invalid_collection("list_size", "null list");
        return 0;
    }
    return (int32_t)list->count;
}

void
pgy_list_remove_raw_export(void *list_ptr, int32_t index, int64_t elem_size)
{
    PgyListRaw *list = (PgyListRaw *)list_ptr;
    size_t tail_count;
    if (list == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list remove on null list");
    }
    if (elem_size <= 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list remove with invalid element size");
    }
    if (index < 0 || (size_t)index >= list->count) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "list remove index out of bounds");
    }
    tail_count = list->count - (size_t)index - 1;
    if (tail_count > 0) {
        memmove((char *)list->data + ((size_t)index * (size_t)elem_size),
                (char *)list->data + (((size_t)index + 1) * (size_t)elem_size),
                tail_count * (size_t)elem_size);
    }
    list->count--;
}

#endif /* PGY_RUNTIME_LIB_LIST_RAW_EXPORTS_H */
