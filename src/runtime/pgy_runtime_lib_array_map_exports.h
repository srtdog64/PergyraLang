/* =================================================================
 * Array operations - extern wrappers for LLVM linker
 * ================================================================= */

#define PGY_DEFINE_ARRAY_EXPORTS(Suffix, CType)                                  \
typedef struct {                                                                 \
    CType  *data;                                                                \
    size_t  length;                                                              \
    size_t  capacity;                                                            \
    void   *allocator;                                                           \
} PgyArray_##Suffix;                                                             \
                                                                                 \
typedef struct {                                                                 \
    CType  *data;                                                                \
    size_t  length;                                                              \
} PgySlice_##Suffix;                                                             \
                                                                                 \
PgyArray_##Suffix pgy_array_new_##Suffix(size_t capacity)                        \
{                                                                                \
    PgyArray_##Suffix arr;                                                       \
    arr.length = 0;                                                              \
    arr.capacity = capacity;                                                     \
    arr.allocator = NULL;                                                        \
    if (capacity > 0 && capacity > SIZE_MAX / sizeof(CType)) {                   \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,                           \
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);           \
    }                                                                            \
    arr.data = capacity > 0                                                      \
        ? (CType *)malloc(sizeof(CType) * capacity)                              \
        : NULL;                                                                  \
    if (arr.data == NULL && capacity > 0) {                                       \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,                           \
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);           \
    }                                                                            \
    return arr;                                                                  \
}                                                                                \
                                                                                 \
void pgy_array_push_##Suffix(PgyArray_##Suffix *arr, CType value)                \
{                                                                                \
    if (arr == NULL)                                                             \
        return;                                                                  \
    if (arr->length == arr->capacity) {                                          \
        size_t next = arr->capacity == 0 ? 4 : arr->capacity * 2;                \
        if (next < arr->capacity || next > SIZE_MAX / sizeof(CType)) {           \
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,                       \
                              PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);       \
        }                                                                        \
        CType *next_data = arr->data == NULL                                     \
            ? (CType *)malloc(sizeof(CType) * next)                              \
            : (CType *)realloc(arr->data, sizeof(CType) * next);                 \
        if (next_data == NULL) {                                                 \
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,                       \
                              PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);       \
        }                                                                        \
        arr->data = next_data;                                                   \
        arr->capacity = next;                                                    \
    }                                                                            \
    arr->data[arr->length++] = value;                                            \
}                                                                                \
                                                                                 \
CType pgy_array_get_##Suffix(PgyArray_##Suffix *arr, size_t index)               \
{                                                                                \
    if (arr == NULL) {                                                           \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,            \
                          "array get on null array");                            \
    }                                                                            \
    if (index >= arr->length) {                                                  \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,                 \
                          PGY_RUNTIME_PANIC_REASON_ARRAY_INDEX_OUT_OF_BOUNDS);   \
    }                                                                            \
    return arr->data[index];                                                     \
}                                                                                \
                                                                                 \
void pgy_array_set_##Suffix(PgyArray_##Suffix *arr, size_t index, CType value)   \
{                                                                                \
    if (arr == NULL) {                                                           \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,            \
                          "array set on null array");                            \
    }                                                                            \
    if (index >= arr->length) {                                                  \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,                 \
                          PGY_RUNTIME_PANIC_REASON_ARRAY_INDEX_OUT_OF_BOUNDS);   \
    }                                                                            \
    arr->data[index] = value;                                                    \
}                                                                                \
                                                                                 \
void pgy_array_pop_##Suffix(PgyArray_##Suffix *arr)                              \
{                                                                                \
    if (arr == NULL)                                                             \
        return;                                                                  \
    if (arr->length > 0)                                                         \
        arr->length--;                                                           \
}                                                                                \
                                                                                 \
CType pgy_slice_get_##Suffix(PgySlice_##Suffix *slice, size_t index)             \
{                                                                                \
    if (slice == NULL) {                                                         \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,            \
                          "slice get on null slice");                            \
    }                                                                            \
    if (index >= slice->length) {                                                \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,                 \
                          PGY_RUNTIME_PANIC_REASON_SLICE_OUT_OF_BOUNDS);         \
    }                                                                            \
    return slice->data[index];                                                   \
}                                                                                \
                                                                                 \
PgySlice_##Suffix pgy_array_slice_##Suffix(PgyArray_##Suffix *arr,               \
                                           size_t start, size_t len)             \
{                                                                                \
    PgySlice_##Suffix slice;                                                     \
    slice.data = NULL;                                                           \
    slice.length = 0;                                                            \
    if (arr == NULL) {                                                           \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,            \
                          "array slice on null array");                          \
    }                                                                            \
    if (start > arr->length || len > arr->length - start) {                      \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,                 \
                          PGY_RUNTIME_PANIC_REASON_SLICE_OUT_OF_BOUNDS);         \
    }                                                                            \
    if (len == 0)                                                                \
        return slice;                                                            \
    if (arr->data == NULL) {                                                     \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,            \
                          "array slice on null backing storage");                \
    }                                                                            \
    slice.data = arr->data + start;                                              \
    slice.length = len;                                                          \
    return slice;                                                                \
}

PGY_DEFINE_ARRAY_EXPORTS(Int, int32_t)
PGY_DEFINE_ARRAY_EXPORTS(Long, int64_t)
PGY_DEFINE_ARRAY_EXPORTS(Float, float)
PGY_DEFINE_ARRAY_EXPORTS(Double, double)
PGY_DEFINE_ARRAY_EXPORTS(Bool, bool)
PGY_DEFINE_ARRAY_EXPORTS(String, char *)

void
pgy_map_keys_raw_export(void *map_ptr, void *out_array_ptr)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    PgyArray_String *out = (PgyArray_String *)out_array_ptr;

    if (out == NULL) {
        pgy_runtime_warn_invalid_collection("map_keys", "null output");
        return;
    }

    *out = pgy_array_new_String(map != NULL ? map->count : 0);
    if (map == NULL) {
        pgy_runtime_warn_invalid_collection("map_keys", "null map");
        return;
    }
    if (map->count == 0 || map->keys == NULL || map->occupied == NULL)
        return;

    for (size_t i = 0; i < map->capacity; i++) {
        char *dup_key;

        if (!map->occupied[i] || map->keys[i] == NULL)
            continue;
        dup_key = pgy_runtime_strdup_export(map->keys[i]);
        if (dup_key == NULL) {
            pgy_runtime_warn_invalid_collection("map_keys", "key duplication failed");
            continue;
        }
        pgy_array_push_String(out, dup_key);
    }
}

void
pgy_map_keys_raw_i32_export(void *map_ptr, void *out_array_ptr)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    PgyArray_Int *out = (PgyArray_Int *)out_array_ptr;

    if (out == NULL) {
        pgy_runtime_warn_invalid_collection("map_keys_i32", "null output");
        return;
    }

    *out = pgy_array_new_Int(map != NULL ? map->count : 0);
    if (map == NULL) {
        pgy_runtime_warn_invalid_collection("map_keys_i32", "null map");
        return;
    }
    if (map->count == 0 || map->keys == NULL || map->occupied == NULL)
        return;

    for (size_t i = 0; i < map->capacity; i++) {
        char *end = NULL;
        long parsed;

        if (!map->occupied[i] || map->keys[i] == NULL)
            continue;
        parsed = strtol(map->keys[i], &end, 10);
        if (end == map->keys[i] || (end != NULL && *end != '\0')) {
            pgy_runtime_warn_invalid_collection("map_keys_i32", "invalid stored int key");
            continue;
        }
        pgy_array_push_Int(out, (int32_t)parsed);
    }
}

void
pgy_map_keys_raw_i64_export(void *map_ptr, void *out_array_ptr)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    PgyArray_Long *out = (PgyArray_Long *)out_array_ptr;

    if (out == NULL) {
        pgy_runtime_warn_invalid_collection("map_keys_i64", "null output");
        return;
    }

    *out = pgy_array_new_Long(map != NULL ? map->count : 0);
    if (map == NULL) {
        pgy_runtime_warn_invalid_collection("map_keys_i64", "null map");
        return;
    }
    if (map->count == 0 || map->keys == NULL || map->occupied == NULL)
        return;

    for (size_t i = 0; i < map->capacity; i++) {
        char *end = NULL;
        long long parsed;

        if (!map->occupied[i] || map->keys[i] == NULL)
            continue;
        parsed = strtoll(map->keys[i], &end, 10);
        if (end == map->keys[i] || (end != NULL && *end != '\0')) {
            pgy_runtime_warn_invalid_collection("map_keys_i64", "invalid stored long key");
            continue;
        }
        pgy_array_push_Long(out, (int64_t)parsed);
    }
}

void
pgy_map_keys_raw_bool_export(void *map_ptr, void *out_array_ptr)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    PgyArray_Bool *out = (PgyArray_Bool *)out_array_ptr;

    if (out == NULL) {
        pgy_runtime_warn_invalid_collection("map_keys_bool", "null output");
        return;
    }

    *out = pgy_array_new_Bool(map != NULL ? map->count : 0);
    if (map == NULL) {
        pgy_runtime_warn_invalid_collection("map_keys_bool", "null map");
        return;
    }
    if (map->count == 0 || map->keys == NULL || map->occupied == NULL)
        return;

    for (size_t i = 0; i < map->capacity; i++) {
        bool parsed;

        if (!map->occupied[i] || map->keys[i] == NULL)
            continue;
        if (strcmp(map->keys[i], "true") == 0) {
            parsed = true;
        } else if (strcmp(map->keys[i], "false") == 0) {
            parsed = false;
        } else {
            pgy_runtime_warn_invalid_collection("map_keys_bool", "invalid stored bool key");
            continue;
        }
        pgy_array_push_Bool(out, parsed);
    }
}
