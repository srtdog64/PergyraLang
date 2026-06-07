#ifndef PGY_RUNTIME_LIST_GENERIC_INLINE_H
#define PGY_RUNTIME_LIST_GENERIC_INLINE_H

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
static inline PgyList_##SuffixName pgy_list_new_##SuffixName(void) \
{ \
    PgyList_##SuffixName l; \
    l.capacity = 16; \
    l.count = 0; \
    if (!PGY_RUNTIME_ELEM_CAPACITY_FITS(l.capacity, CType)) { \
        l.data = NULL; l.capacity = 0; \
        pgy_runtime_warn_invalid_collection("list_new_" #SuffixName, "allocation size overflow"); \
        return l; \
    } \
    l.data = (CType *)calloc(l.capacity, sizeof(CType)); \
    if (l.data == NULL) { \
        l.capacity = 0; \
        pgy_runtime_warn_invalid_collection("list_new_" #SuffixName, "allocation failed"); \
    } \
    return l; \
} \
\
static inline void pgy_list_push_##SuffixName(PgyList_##SuffixName *l, CType val) \
{ \
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
        grown = (CType *)realloc(l->data, new_capacity * sizeof(CType)); \
        if (grown == NULL) { \
            pgy_runtime_warn_invalid_collection("list_push_" #SuffixName, "realloc failed"); \
            return; \
        } \
        l->data = grown; \
        l->capacity = new_capacity; \
    } \
    l->data[l->count++] = val; \
} \
\
static inline CType pgy_list_get_##SuffixName(PgyList_##SuffixName *l, int32_t index) \
{ \
    if (!PGY_RUNTIME_LIST_IS_INITIALIZED(l, CType)) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "list get on invalid list"); \
    if (index < 0 || (size_t)index >= l->count) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "list index out of bounds"); \
    return l->data[index]; \
} \
\
static inline void pgy_list_set_##SuffixName(PgyList_##SuffixName *l, int32_t index, CType val) \
{ \
    if (!PGY_RUNTIME_LIST_IS_INITIALIZED(l, CType)) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "list set on invalid list"); \
    if (index < 0 || (size_t)index >= l->count) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "list set index out of bounds"); \
    l->data[index] = val; \
} \
\
static inline int32_t pgy_list_size_##SuffixName(PgyList_##SuffixName *l) \
{ return PGY_RUNTIME_LIST_IS_INITIALIZED(l, CType) ? (int32_t)l->count : 0; } \
\
static inline void pgy_list_remove_##SuffixName(PgyList_##SuffixName *l, int32_t index) \
{ \
    if (!PGY_RUNTIME_LIST_IS_INITIALIZED(l, CType)) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "list remove on invalid list"); \
    if (index < 0 || (size_t)index >= l->count) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "list remove index out of bounds"); \
    for (size_t i = (size_t)index; i < l->count - 1; i++) \
        l->data[i] = l->data[i + 1]; \
    l->count--; \
}

#endif /* PGY_RUNTIME_LIST_GENERIC_INLINE_H */
