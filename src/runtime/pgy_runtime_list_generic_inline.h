#ifndef PGY_RUNTIME_LIST_GENERIC_INLINE_H
#define PGY_RUNTIME_LIST_GENERIC_INLINE_H

#include "pgy_runtime_linkage.h"

/* Growable runtime storage is not a synchronization boundary.
 * The semantic layer must reject raw Array/Slice/List/Queue/Set/HashMap
 * transport across parallel/async/worker boundaries unless an explicit copy or
 * pinned read-only view owner is used.
 */

#define PGY_LIST_DEFINE(SuffixName, CType) \
typedef struct \
{ \
    CType   *data; \
    size_t   count; \
    size_t   capacity; \
} PgyList_##SuffixName; \
\
PGY_RT_PROGRAM_DECL PgyList_##SuffixName pgy_list_new_##SuffixName(void) \
PGY_RT_PROGRAM_BODY({ \
    PgyList_##SuffixName l; \
    l.capacity = 16; \
    l.count = 0; \
    if (!PGY_RUNTIME_ELEM_CAPACITY_FITS(l.capacity, CType)) { \
        l.data = NULL; l.capacity = 0; \
        pgy_runtime_warn_invalid_collection("list_new_" #SuffixName, "allocation size overflow"); \
        return l; \
    } \
    pgy_budget_charge_alloc(l.capacity * sizeof(CType)); \
    l.data = (CType *)calloc(l.capacity, sizeof(CType)); \
    if (l.data == NULL) { \
        l.capacity = 0; \
        pgy_runtime_warn_invalid_collection("list_new_" #SuffixName, "allocation failed"); \
    } \
    return l; \
}) \
\
PGY_RT_PROGRAM_DECL void pgy_list_push_##SuffixName(PgyList_##SuffixName *l, CType val) \
PGY_RT_PROGRAM_BODY({ \
    if (!PGY_RUNTIME_LIST_IS_INITIALIZED(l, CType)) { \
        pgy_runtime_warn_invalid_collection("list_push_" #SuffixName, "list is not initialized"); \
        return; \
    } \
    if (l->count >= l->capacity) { \
        size_t new_capacity; \
        CType *grown; \
        if (l->capacity == 0) { \
            new_capacity = 16; \
        } else { \
            if (l->capacity > SIZE_MAX / 2) { \
                pgy_runtime_warn_invalid_collection("list_push_" #SuffixName, "capacity overflow"); \
                return; \
            } \
            new_capacity = l->capacity * 2; \
        } \
        if (!PGY_RUNTIME_ELEM_CAPACITY_FITS(new_capacity, CType)) { \
            pgy_runtime_warn_invalid_collection("list_push_" #SuffixName, "allocation size overflow"); \
            return; \
        } \
        pgy_budget_charge_alloc((new_capacity - l->capacity) * sizeof(CType)); \
        grown = (CType *)realloc(l->data, new_capacity * sizeof(CType)); \
        if (grown == NULL) { \
            pgy_runtime_warn_invalid_collection("list_push_" #SuffixName, "realloc failed"); \
            return; \
        } \
        l->data = grown; \
        l->capacity = new_capacity; \
    } \
    l->data[l->count++] = val; \
}) \
\
PGY_RT_PROGRAM_DECL CType pgy_list_get_##SuffixName(PgyList_##SuffixName *l, int32_t index) \
PGY_RT_PROGRAM_BODY({ \
    if (!PGY_RUNTIME_LIST_IS_INITIALIZED(l, CType)) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "list get on invalid list"); \
    if (index < 0 || (size_t)index >= l->count) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "list index out of bounds"); \
    return l->data[index]; \
}) \
\
PGY_RT_PROGRAM_DECL void pgy_list_set_##SuffixName(PgyList_##SuffixName *l, int32_t index, CType val) \
PGY_RT_PROGRAM_BODY({ \
    if (!PGY_RUNTIME_LIST_IS_INITIALIZED(l, CType)) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "list set on invalid list"); \
    if (index < 0 || (size_t)index >= l->count) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "list set index out of bounds"); \
    l->data[index] = val; \
}) \
\
PGY_RT_PROGRAM_DECL int32_t pgy_list_size_##SuffixName(PgyList_##SuffixName *l) \
PGY_RT_PROGRAM_BODY({ return PGY_RUNTIME_LIST_IS_INITIALIZED(l, CType) ? (int32_t)l->count : 0; }) \
\
PGY_RT_PROGRAM_DECL void pgy_list_remove_##SuffixName(PgyList_##SuffixName *l, int32_t index) \
PGY_RT_PROGRAM_BODY({ \
    if (!PGY_RUNTIME_LIST_IS_INITIALIZED(l, CType)) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "list remove on invalid list"); \
    if (index < 0 || (size_t)index >= l->count) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "list remove index out of bounds"); \
    for (size_t i = (size_t)index; i < l->count - 1; i++) \
        l->data[i] = l->data[i + 1]; \
    l->count--; \
})

#endif /* PGY_RUNTIME_LIST_GENERIC_INLINE_H */
