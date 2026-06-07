#ifndef PGY_RUNTIME_LIB_LIST_RAW_EXPORTS_H
#define PGY_RUNTIME_LIB_LIST_RAW_EXPORTS_H

/* LLVM-linkable raw List<T> exports used by generic collection lowering. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Growable runtime storage is not a synchronization boundary.
 * The semantic layer must reject raw Array/Slice/List/Queue/Set/HashMap
 * transport across parallel/async/worker boundaries unless an explicit copy or
 * pinned read-only view owner is used.
 */

typedef struct {
    void   *data;
    size_t  count;
    size_t  capacity;
} PgyListRaw;

static char *pgy_runtime_strdup_export(const char *src);

static bool
pgy_list_raw_shape_fits(size_t capacity, size_t elem_size)
{
    return capacity != 0
        && capacity <= (size_t)INT32_MAX
        && elem_size != 0
        && elem_size <= SIZE_MAX / capacity;
}

static bool
pgy_list_raw_is_initialized(const PgyListRaw *list)
{
    return list != NULL
        && list->capacity != 0
        && list->capacity <= (size_t)INT32_MAX
        && list->data != NULL;
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
    if (!pgy_list_raw_is_initialized(list)) {
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
pgy_list_push_string_raw_export(void *list_ptr, const char *value)
{
    PgyListRaw *list = (PgyListRaw *)list_ptr;
    size_t before_count;
    char *owned;

    if (list == NULL) {
        pgy_runtime_warn_invalid_collection("list_push_string", "null list");
        return;
    }
    if (!pgy_list_raw_is_initialized(list)
        || !pgy_list_raw_shape_fits(list->capacity, sizeof(char *))) {
        pgy_runtime_warn_invalid_collection("list_push_string",
            "list is not initialized");
        return;
    }
    owned = pgy_runtime_strdup_export(value != NULL ? value : "");
    if (owned == NULL) {
        pgy_runtime_warn_invalid_collection("list_push_string", "string duplication failed");
        return;
    }
    before_count = list->count;
    pgy_list_push_raw_export(list_ptr, &owned, (int64_t)sizeof(char *));
    if (list->count == before_count)
        free(owned);
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
    if (!pgy_list_raw_is_initialized(list)
        || !pgy_list_raw_shape_fits(list->capacity, (size_t)elem_size)) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list get on uninitialized list");
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
pgy_list_get_string_raw_export(void *list_ptr, int32_t index, char **out_ptr)
{
    if (out_ptr == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list get string on null output");
    }
    pgy_list_get_raw_export(list_ptr, index, out_ptr, (int64_t)sizeof(char *));
    if (*out_ptr == NULL)
        *out_ptr = "";
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
    if (!pgy_list_raw_is_initialized(list)
        || !pgy_list_raw_shape_fits(list->capacity, (size_t)elem_size)) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list set on uninitialized list");
    }
    if (index < 0 || (size_t)index >= list->count) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "list set index out of bounds");
    }
    memcpy((char *)list->data + ((size_t)index * (size_t)elem_size),
           value_ptr, (size_t)elem_size);
}

void
pgy_list_set_string_raw_export(void *list_ptr, int32_t index, const char *value)
{
    PgyListRaw *list = (PgyListRaw *)list_ptr;
    char *owned;
    char **slot;

    if (list == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list set string on null list");
    }
    if (!pgy_list_raw_is_initialized(list)
        || !pgy_list_raw_shape_fits(list->capacity, sizeof(char *))) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list set string on uninitialized list");
    }
    if (index < 0 || (size_t)index >= list->count) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "list set string index out of bounds");
    }
    owned = pgy_runtime_strdup_export(value != NULL ? value : "");
    if (owned == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          "list set string duplication failed");
    }
    slot = (char **)((char *)list->data + ((size_t)index * sizeof(char *)));
    free(*slot);
    *slot = owned;
}

int32_t
pgy_list_size_raw_export(void *list_ptr)
{
    PgyListRaw *list = (PgyListRaw *)list_ptr;
    if (list == NULL) {
        pgy_runtime_warn_invalid_collection("list_size", "null list");
        return 0;
    }
    if (!pgy_list_raw_is_initialized(list)) {
        pgy_runtime_warn_invalid_collection("list_size", "list is not initialized");
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
    if (!pgy_list_raw_is_initialized(list)
        || !pgy_list_raw_shape_fits(list->capacity, (size_t)elem_size)) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list remove on uninitialized list");
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

void
pgy_list_remove_string_raw_export(void *list_ptr, int32_t index)
{
    PgyListRaw *list = (PgyListRaw *)list_ptr;
    char **slot;
    size_t tail_count;

    if (list == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list remove string on null list");
    }
    if (!pgy_list_raw_is_initialized(list)
        || !pgy_list_raw_shape_fits(list->capacity, sizeof(char *))) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list remove string on uninitialized list");
    }
    if (index < 0 || (size_t)index >= list->count) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "list remove string index out of bounds");
    }
    slot = (char **)((char *)list->data + ((size_t)index * sizeof(char *)));
    free(*slot);
    *slot = NULL;
    tail_count = list->count - (size_t)index - 1;
    if (tail_count > 0) {
        memmove(slot, slot + 1, tail_count * sizeof(char *));
        ((char **)list->data)[list->count - 1] = NULL;
    }
    list->count--;
}

#endif /* PGY_RUNTIME_LIB_LIST_RAW_EXPORTS_H */
