/* =================================================================
 * Secure slot operations — extern wrappers for LLVM linker
 * ================================================================= */

#define PGY_DEFINE_SECURE_SLOT_EXPORTS(Suffix, CType, ZeroExpr)                \
static uint64_t pgy_secure_token_counter_##Suffix = 0x9e3779b97f4a7c15ULL;     \
                                                                               \
typedef struct {                                                               \
    CType    value;                                                            \
    bool     occupied;                                                         \
    uint64_t token;                                                            \
} PgySecureSlot_##Suffix;                                                      \
                                                                               \
typedef struct {                                                               \
    uint64_t id;                                                               \
    bool     can_write;                                                        \
    bool     can_read;                                                         \
} PgyToken_##Suffix;                                                           \
                                                                               \
PgySecureSlot_##Suffix pgy_claim_secure_##Suffix(PgyToken_##Suffix *out_token) \
{                                                                              \
    PgySecureSlot_##Suffix s;                                                  \
    uint64_t id = ++pgy_secure_token_counter_##Suffix;                         \
    if (id == 0) {                                                             \
        id = ++pgy_secure_token_counter_##Suffix;                              \
    }                                                                          \
    s.value = (ZeroExpr);                                                      \
    s.occupied = true;                                                         \
    s.token = id;                                                              \
    if (out_token != NULL) {                                                   \
        out_token->id = id;                                                    \
        out_token->can_write = true;                                           \
        out_token->can_read = true;                                            \
    }                                                                          \
    return s;                                                                  \
}                                                                              \
                                                                               \
void pgy_secure_write_##Suffix(PgySecureSlot_##Suffix *s, CType v,             \
                               const PgyToken_##Suffix *t)                     \
{                                                                              \
    if (s == NULL || t == NULL)                                                 \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,          \
                          "null secure slot write operand");                   \
    if (!s->occupied)                                                           \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,               \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_WRITE); \
    if (s->token != t->id)                                                      \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN,         \
                          PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_WRITE); \
    if (!t->can_write)                                                          \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN,         \
                          PGY_RUNTIME_PANIC_REASON_SECURE_TOKEN_DENIES_WRITE);  \
    s->value = v;                                                              \
}                                                                              \
                                                                               \
CType pgy_secure_read_##Suffix(PgySecureSlot_##Suffix *s,                      \
                               const PgyToken_##Suffix *t)                     \
{                                                                              \
    if (s == NULL || t == NULL)                                                 \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,          \
                          "null secure slot read operand");                    \
    if (!s->occupied)                                                           \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,               \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_READ);  \
    if (s->token != t->id)                                                      \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN,         \
                          PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_READ);  \
    if (!t->can_read)                                                           \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN,         \
                          PGY_RUNTIME_PANIC_REASON_SECURE_TOKEN_DENIES_READ);   \
    return s->value;                                                           \
}                                                                              \
                                                                               \
void pgy_secure_release_##Suffix(PgySecureSlot_##Suffix *s,                    \
                                 const PgyToken_##Suffix *t)                   \
{                                                                              \
    if (s == NULL || t == NULL)                                                 \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,          \
                          "null secure slot release operand");                 \
    if (!s->occupied)                                                           \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE,              \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_RELEASE); \
    if (s->token != t->id)                                                      \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN,         \
                          PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_RELEASE); \
    s->occupied = false;                                                       \
    s->token = 0;                                                              \
}                                                                              \
                                                                               \
typedef struct {                                                               \
    PgySecureSlot_##Suffix  *slot;                                             \
    const PgyToken_##Suffix *token;                                            \
    bool                     active;                                           \
    bool                     can_write;                                        \
} PgyPinnedSecureSlotView_##Suffix;                                            \
                                                                               \
PgyPinnedSecureSlotView_##Suffix                                               \
pgy_secure_pin_read_##Suffix(PgySecureSlot_##Suffix *s,                        \
                             const PgyToken_##Suffix *t)                       \
{                                                                              \
    if (s == NULL || t == NULL)                                                 \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,          \
                          "null secure slot pin read operand");                \
    if (!s->occupied)                                                           \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,               \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_READ);  \
    if (s->token != t->id)                                                      \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN,         \
                          PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_READ);  \
    if (!t->can_read)                                                           \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN,         \
                          PGY_RUNTIME_PANIC_REASON_SECURE_TOKEN_DENIES_READ);   \
    PgyPinnedSecureSlotView_##Suffix view;                                      \
    view.slot = s;                                                             \
    view.token = t;                                                            \
    view.active = true;                                                        \
    view.can_write = false;                                                    \
    return view;                                                               \
}                                                                              \
                                                                               \
PgyPinnedSecureSlotView_##Suffix                                               \
pgy_secure_pin_write_##Suffix(PgySecureSlot_##Suffix *s,                       \
                              const PgyToken_##Suffix *t)                      \
{                                                                              \
    if (s == NULL || t == NULL)                                                 \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,          \
                          "null secure slot pin write operand");               \
    if (!s->occupied)                                                           \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,               \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_WRITE); \
    if (s->token != t->id)                                                      \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN,         \
                          PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_WRITE); \
    if (!t->can_write)                                                          \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN,         \
                          PGY_RUNTIME_PANIC_REASON_SECURE_TOKEN_DENIES_WRITE);  \
    PgyPinnedSecureSlotView_##Suffix view;                                      \
    view.slot = s;                                                             \
    view.token = t;                                                            \
    view.active = true;                                                        \
    view.can_write = true;                                                     \
    return view;                                                               \
}                                                                              \
                                                                               \
void pgy_secure_unpin_##Suffix(PgyPinnedSecureSlotView_##Suffix *view)         \
{                                                                              \
    if (view == NULL)                                                           \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,          \
                          "null secure slot unpin");                           \
    if (!view->active || view->slot == NULL || view->token == NULL)             \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,          \
                          "inactive secure slot unpin");                       \
    view->active = false;                                                      \
    view->slot = NULL;                                                         \
    view->token = NULL;                                                        \
}

PGY_DEFINE_SECURE_SLOT_EXPORTS(Int, int32_t, 0)
PGY_DEFINE_SECURE_SLOT_EXPORTS(Long, int64_t, 0)
PGY_DEFINE_SECURE_SLOT_EXPORTS(Float, float, 0.0f)
PGY_DEFINE_SECURE_SLOT_EXPORTS(Double, double, 0.0)
PGY_DEFINE_SECURE_SLOT_EXPORTS(Bool, bool, false)
PGY_DEFINE_SECURE_SLOT_EXPORTS(String, char *, NULL)

/* =================================================================
 * Device Slot operations — extern wrappers for LLVM linker
 * ================================================================= */

#define PGY_DEFINE_DEVICE_SLOT_EXPORTS(Suffix, CType, ZeroExpr)                 \
typedef struct {                                                                \
    CType value;                                                                \
    bool  claimed;                                                              \
} PgyDeviceSlot_##Suffix;                                                       \
                                                                                \
typedef struct {                                                                \
    PgyDeviceSlot_##Suffix *slot;                                               \
} PgyDeviceReadTaskArg_##Suffix;                                                \
                                                                                \
PgyDeviceSlot_##Suffix pgy_claim_device_##Suffix(void)                          \
{                                                                               \
    PgyDeviceSlot_##Suffix s;                                                   \
    s.value = (ZeroExpr);                                                       \
    s.claimed = true;                                                           \
    return s;                                                                   \
}                                                                               \
                                                                                \
void pgy_device_write_##Suffix(PgyDeviceSlot_##Suffix *s, CType v)              \
{                                                                               \
    if (s == NULL || !s->claimed) {                                             \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,                \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_DEVICE_SLOT_WRITE); \
    }                                                                           \
    s->value = v;                                                               \
}                                                                               \
                                                                                \
CType pgy_device_read_##Suffix(PgyDeviceSlot_##Suffix *s)                       \
{                                                                               \
    if (s == NULL || !s->claimed) {                                             \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,                \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_DEVICE_SLOT_READ);  \
    }                                                                           \
    return s->value;                                                            \
}                                                                               \
                                                                                \
void pgy_release_device_##Suffix(PgyDeviceSlot_##Suffix *s)                     \
{                                                                               \
    if (s == NULL) {                                                            \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,           \
                          "device slot release on null slot");                 \
    }                                                                           \
    if (!s->claimed) {                                                          \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE,               \
                          PGY_RUNTIME_PANIC_REASON_DOUBLE_RELEASE_DEVICE_SLOT); \
    }                                                                           \
    s->value = (ZeroExpr);                                                      \
    s->claimed = false;                                                         \
}                                                                               \
                                                                                \
static void *pgy_device_read_task_##Suffix(void *raw)                           \
{                                                                               \
    PgyDeviceReadTaskArg_##Suffix *arg =                                        \
        (PgyDeviceReadTaskArg_##Suffix *)raw;                                   \
    CType *result = (CType *)malloc(sizeof(CType));                             \
    if (result == NULL) {                                                       \
        free(arg);                                                              \
        return NULL;                                                            \
    }                                                                           \
    *result = pgy_device_read_##Suffix(arg->slot);                              \
    free(arg);                                                                  \
    return result;                                                              \
}                                                                               \
                                                                                \
PgyTaskHandle pgy_submit_device_read_##Suffix(PgyDeviceSlot_##Suffix *s)        \
{                                                                               \
    PgyDeviceReadTaskArg_##Suffix *arg =                                        \
        (PgyDeviceReadTaskArg_##Suffix *)malloc(sizeof(PgyDeviceReadTaskArg_##Suffix)); \
    if (arg == NULL) {                                                          \
        PgyTaskHandle empty = {0};                                              \
        return empty;                                                           \
    }                                                                           \
    arg->slot = s;                                                              \
    return pgy_spawn(pgy_device_read_task_##Suffix, arg);                       \
}

PGY_DEFINE_DEVICE_SLOT_EXPORTS(Int, int32_t, 0)
PGY_DEFINE_DEVICE_SLOT_EXPORTS(Long, int64_t, 0)
PGY_DEFINE_DEVICE_SLOT_EXPORTS(Float, float, 0.0f)
PGY_DEFINE_DEVICE_SLOT_EXPORTS(Double, double, 0.0)
PGY_DEFINE_DEVICE_SLOT_EXPORTS(Bool, bool, false)
PGY_DEFINE_DEVICE_SLOT_EXPORTS(String, char *, NULL)

/* =================================================================
 * Array operations — extern wrappers for LLVM linker
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
    if (start + len > arr->length) {                                             \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,                 \
                          PGY_RUNTIME_PANIC_REASON_SLICE_OUT_OF_BOUNDS);         \
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

static char *
pgy_runtime_lib_strdup(const char *src)
{
    if (src == NULL)
        src = "";

    size_t len = strlen(src);
    char *copy = (char *)malloc(len + 1);
    if (copy == NULL)
        return NULL;

    memcpy(copy, src, len + 1);
    return copy;
}

/* =================================================================
 * File I/O and string helpers needed by LLVM backend
 * ================================================================= */

#define PGY_MAX_OPEN_FILES 256

static FILE *pgy_runtime_ftable[PGY_MAX_OPEN_FILES];
static int   pgy_runtime_ftable_next = 3;

static void
pgy_runtime_io_init(void)
{
    pgy_runtime_ftable[0] = stdin;
    pgy_runtime_ftable[1] = stdout;
    pgy_runtime_ftable[2] = stderr;
}

int32_t pgy_file_open(const char *path, const char *mode)
{
    if (pgy_runtime_ftable[0] == NULL)
        pgy_runtime_io_init();

    FILE *fp = fopen(path, mode);
    if (fp == NULL)
        return -1;

    int fd = -1;
    for (int i = 3; i < PGY_MAX_OPEN_FILES; i++) {
        if (pgy_runtime_ftable[i] == NULL) {
            fd = i;
            break;
        }
    }
    if (fd < 0) {
        fclose(fp);
        return -1;
    }

    if (fd >= pgy_runtime_ftable_next)
        pgy_runtime_ftable_next = fd + 1;
    pgy_runtime_ftable[fd] = fp;
    return (int32_t)fd;
}

char *pgy_file_read(int32_t fd)
{
    char tmp[4096];

    tmp[0] = '\0';
    if (fd < 0 || fd >= PGY_MAX_OPEN_FILES || pgy_runtime_ftable[fd] == NULL)
        return pgy_runtime_lib_strdup("");
    if (fgets(tmp, sizeof(tmp), pgy_runtime_ftable[fd]) == NULL)
        return pgy_runtime_lib_strdup("");

    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '\n')
        tmp[len - 1] = '\0';
    return pgy_runtime_lib_strdup(tmp);
}

void pgy_file_write(int32_t fd, const char *data)
{
    if (fd < 0 || fd >= PGY_MAX_OPEN_FILES || pgy_runtime_ftable[fd] == NULL)
        return;
    if (data != NULL)
        fwrite(data, 1, strlen(data), pgy_runtime_ftable[fd]);
}

void pgy_file_close(int32_t fd)
{
    if (fd < 3 || fd >= PGY_MAX_OPEN_FILES || pgy_runtime_ftable[fd] == NULL)
        return;
    fclose(pgy_runtime_ftable[fd]);
    pgy_runtime_ftable[fd] = NULL;
}

char *pgy_read_file(const char *path)
{
    char *resolved = pgy_runtime_resolve_file_path(path, false);
    if (resolved == NULL)
        return pgy_runtime_lib_strdup("");

    FILE *fp = fopen(resolved, "rb");
    if (fp == NULL)
        return pgy_runtime_lib_strdup("");

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return pgy_runtime_lib_strdup("");
    }
    long len = ftell(fp);
    if (len < 0 || (unsigned long)len > (unsigned long)PGY_RUNTIME_MAX_FILE_BYTES) {
        fclose(fp);
        return pgy_runtime_lib_strdup("");
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return pgy_runtime_lib_strdup("");
    }

    char *buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL) {
        fclose(fp);
        return pgy_runtime_lib_strdup("");
    }

    size_t read_len = fread(buf, 1, (size_t)len, fp);
    if (read_len != (size_t)len) {
        fclose(fp);
        free(resolved);
        free(buf);
        return pgy_runtime_lib_strdup("");
    }
    buf[read_len] = '\0';
    fclose(fp);
    free(resolved);
    return buf;
}

void pgy_write_file(const char *path, const char *data)
{
    char *resolved = pgy_runtime_resolve_file_path(path, true);
    if (resolved == NULL)
        return;
    FILE *fp = fopen(resolved, "wb");
    if (fp == NULL)
        return;
    if (data != NULL) {
        size_t len = strlen(data);
        (void)fwrite(data, 1, len, fp);
    }
    fclose(fp);
    free(resolved);
}

char *pgy_input(const char *prompt)
{
    char tmp[4096];

    if (prompt != NULL && prompt[0] != '\0')
        printf("%s", prompt);
    fflush(stdout);

    tmp[0] = '\0';
    if (fgets(tmp, sizeof(tmp), stdin) == NULL) {
        char *empty = (char *)malloc(1);
        if (empty != NULL) empty[0] = '\0';
        return empty;
    }

    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '\n')
        tmp[len - 1] = '\0';

    len = strlen(tmp);
    char *result = (char *)malloc(len + 1);
    if (result != NULL)
        memcpy(result, tmp, len + 1);
    return result;
}

bool StringContains(const char *haystack, const char *needle)
{
    if (haystack == NULL || needle == NULL)
        return false;
    return strstr(haystack, needle) != NULL;
}

char *Substring(const char *s, int32_t start, int32_t len)
{
    if (s == NULL)
        return pgy_runtime_lib_strdup("");

    int32_t slen = (int32_t)strlen(s);
    if (start < 0 || start >= slen || len <= 0)
        return pgy_runtime_lib_strdup("");
    if (start + len > slen)
        len = slen - start;

    char *buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL)
        return pgy_runtime_lib_strdup("");
    memcpy(buf, s + start, (size_t)len);
    buf[len] = '\0';
    return buf;
}

char *StringReplace(const char *s, const char *old_str, const char *new_str)
{
    if (s == NULL || old_str == NULL || new_str == NULL)
        return pgy_runtime_lib_strdup(s != NULL ? s : "");

    size_t old_len = strlen(old_str);
    size_t new_len = strlen(new_str);
    if (old_len == 0)
        return pgy_runtime_lib_strdup(s);

    int count = 0;
    const char *p = s;
    while ((p = strstr(p, old_str)) != NULL) {
        count++;
        p += old_len;
    }

    size_t result_len = strlen(s) + (size_t)count * (new_len - old_len);
    char *result = (char *)malloc(result_len + 1);
    char *dst = result;

    if (result == NULL)
        return pgy_runtime_lib_strdup("");

    p = s;
    while (*p) {
        if (strncmp(p, old_str, old_len) == 0) {
            memcpy(dst, new_str, new_len);
            dst += new_len;
            p += old_len;
        } else {
            *dst++ = *p++;
        }
    }
    *dst = '\0';
    return result;
}

char *StringTrim(const char *s)
{
    if (s == NULL)
        return pgy_runtime_lib_strdup("");

    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;

    size_t len = strlen(s);
    while (len > 0
           && (s[len - 1] == ' ' || s[len - 1] == '\t'
               || s[len - 1] == '\n' || s[len - 1] == '\r'))
        len--;

    char *buf = (char *)malloc(len + 1);
    if (buf == NULL)
        return pgy_runtime_lib_strdup("");
    memcpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}

char *ToUpper(const char *s)
{
    if (s == NULL)
        return pgy_runtime_lib_strdup("");

    size_t len = strlen(s);
    char *buf = (char *)malloc(len + 1);
    if (buf == NULL)
        return pgy_runtime_lib_strdup("");
    for (size_t i = 0; i <= len; i++)
        buf[i] = (s[i] >= 'a' && s[i] <= 'z') ? (char)(s[i] - 32) : s[i];
    return buf;
}

char *ToLower(const char *s)
{
    if (s == NULL)
        return pgy_runtime_lib_strdup("");

    size_t len = strlen(s);
    char *buf = (char *)malloc(len + 1);
    if (buf == NULL)
        return pgy_runtime_lib_strdup("");
    for (size_t i = 0; i <= len; i++)
        buf[i] = (s[i] >= 'A' && s[i] <= 'Z') ? (char)(s[i] + 32) : s[i];
    return buf;
}

char *StringConcat(const char *a, const char *b)
{
    if (a == NULL)
        a = "";
    if (b == NULL)
        b = "";

    size_t la = strlen(a);
    size_t lb = strlen(b);
    char *buf = (char *)malloc(la + lb + 1);
    if (buf == NULL)
        return pgy_runtime_lib_strdup("");
    memcpy(buf, a, la);
    memcpy(buf + la, b, lb + 1);
    return buf;
}

bool pgy_string_equals(const char *a, const char *b)
{
    if (a == NULL)
        a = "";
    if (b == NULL)
        b = "";
    return strcmp(a, b) == 0;
}

/* -----------------------------------------------------------------
 * StringSplit / StringJoin / ToInt / ToFloat / Math
 * ----------------------------------------------------------------- */

/* StringSplit(str, delim) → Array<String> (caller-allocated PgyArray_String) */
PgyArray_String StringSplit(const char *s, const char *delim)
{
    PgyArray_String result = pgy_array_new_String(8);
    if (s == NULL || delim == NULL || *delim == '\0') {
        if (s != NULL)
            pgy_array_push_String(&result, pgy_runtime_lib_strdup(s));
        return result;
    }
    size_t dlen = strlen(delim);
    const char *p = s;
    for (;;) {
        const char *found = strstr(p, delim);
        if (found == NULL) {
            pgy_array_push_String(&result, pgy_runtime_lib_strdup(p));
            break;
        }
        size_t seg = (size_t)(found - p);
        char *part = (char *)malloc(seg + 1);
        if (part != NULL) { memcpy(part, p, seg); part[seg] = '\0'; }
        pgy_array_push_String(&result, part != NULL ? part : pgy_runtime_lib_strdup(""));
        p = found + dlen;
    }
    return result;
}
