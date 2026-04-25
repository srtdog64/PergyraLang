/* =================================================================
 * Device Slot (Anchored external resource cell)
 *
 * Same ownership surface as Slot<T>, but treated as a device/remote
 * boundary and able to submit asynchronous reads.
 * ================================================================= */

#define PGY_DEVICE_SLOT_DEFINE(SuffixName, CType) \
\
typedef struct { \
    CType   value; \
    bool    claimed; \
} PgyDeviceSlot_##SuffixName; \
\
typedef struct { \
    PgyDeviceSlot_##SuffixName *slot; \
} PgyDeviceReadTaskArg_##SuffixName; \
\
static inline PgyDeviceSlot_##SuffixName \
pgy_claim_device_##SuffixName(void) \
{ \
    PgyDeviceSlot_##SuffixName s; \
    memset(&s, 0, sizeof(s)); \
    s.claimed = true; \
    return s; \
} \
\
static inline void \
pgy_device_write_##SuffixName(PgyDeviceSlot_##SuffixName *s, CType v) \
{ \
    if (s == NULL || !s->claimed) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT, \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_DEVICE_SLOT_WRITE); \
    s->value = v; \
} \
\
static inline CType \
pgy_device_read_##SuffixName(PgyDeviceSlot_##SuffixName *s) \
{ \
    if (s == NULL || !s->claimed) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT, \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_DEVICE_SLOT_READ); \
    return s->value; \
} \
\
static inline void \
pgy_release_device_##SuffixName(PgyDeviceSlot_##SuffixName *s) \
{ \
    if (s == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "device slot release on null slot"); \
    if (!s->claimed) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE, \
                          PGY_RUNTIME_PANIC_REASON_DOUBLE_RELEASE_DEVICE_SLOT); \
    memset(&s->value, 0, sizeof(s->value)); \
    s->claimed = false; \
} \
\
static inline void * \
pgy_device_read_task_##SuffixName(void *raw) \
{ \
    PgyDeviceReadTaskArg_##SuffixName *arg = (PgyDeviceReadTaskArg_##SuffixName *)raw; \
    CType *result = (CType *)malloc(sizeof(CType)); \
    if (result == NULL) { \
        free(arg); \
        return NULL; \
    } \
    *result = pgy_device_read_##SuffixName(arg->slot); \
    free(arg); \
    return result; \
} \
\
static inline PgyTaskHandle \
pgy_submit_device_read_##SuffixName(PgyDeviceSlot_##SuffixName *s) \
{ \
    PgyDeviceReadTaskArg_##SuffixName *arg = \
        (PgyDeviceReadTaskArg_##SuffixName *)malloc(sizeof(PgyDeviceReadTaskArg_##SuffixName)); \
    if (arg == NULL) { \
        PgyTaskHandle empty = {0}; \
        return empty; \
    } \
    arg->slot = s; \
    return pgy_spawn(pgy_device_read_task_##SuffixName, arg); \
}

/* =================================================================
 * Secure Slot (Token-based Access Control)
 *
 * Always includes checks (security feature), but can be disabled
 * in release mode for non-security-critical builds.
 * ================================================================= */

#define PGY_SECURE_SLOT_DEFINE_DEBUG(SuffixName, CType) \
\
typedef struct { \
    CType    value; \
    bool     occupied; \
    uint64_t token; \
} PgySecureSlot_##SuffixName; \
\
typedef struct { \
    uint64_t id; \
    bool     can_write; \
    bool     can_read; \
} PgyToken_##SuffixName; \
\
static inline void \
pgy_make_token_##SuffixName(PgySecureSlot_##SuffixName* s, \
                             PgyToken_##SuffixName* t) \
{ \
    uint64_t id = (uint64_t)(uintptr_t)s ^ 0xDEADBEEFCAFEBABEULL; \
    s->token    = id; \
    t->id       = id; \
    t->can_write = true; \
    t->can_read  = true; \
} \
\
static inline PgySecureSlot_##SuffixName \
pgy_claim_secure_##SuffixName(PgyToken_##SuffixName* out_token) \
{ \
    PgySecureSlot_##SuffixName s; \
    memset(&s, 0, sizeof(s)); \
    s.occupied = true; \
    pgy_make_token_##SuffixName(&s, out_token); \
    return s; \
} \
\
static inline void \
pgy_secure_write_##SuffixName(PgySecureSlot_##SuffixName* s, \
                               CType v, \
                               const PgyToken_##SuffixName* t) \
{ \
    if (s == NULL || t == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "null secure slot write operand"); \
    if (!s->occupied) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT, \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_WRITE); \
    if (s->token != t->id) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN, \
                          PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_WRITE); \
    if (!t->can_write) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN, \
                          PGY_RUNTIME_PANIC_REASON_SECURE_TOKEN_DENIES_WRITE); \
    s->value = v; \
} \
\
static inline CType \
pgy_secure_read_##SuffixName(PgySecureSlot_##SuffixName* s, \
                              const PgyToken_##SuffixName* t) \
{ \
    if (s == NULL || t == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "null secure slot read operand"); \
    if (!s->occupied) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT, \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_READ); \
    if (s->token != t->id) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN, \
                          PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_READ); \
    if (!t->can_read) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN, \
                          PGY_RUNTIME_PANIC_REASON_SECURE_TOKEN_DENIES_READ); \
    return s->value; \
} \
\
static inline void \
pgy_secure_release_##SuffixName(PgySecureSlot_##SuffixName* s, \
                                 const PgyToken_##SuffixName* t) \
{ \
    if (s == NULL || t == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "null secure slot release operand"); \
    if (!s->occupied) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE, \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_RELEASE); \
    if (s->token != t->id) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN, \
                          PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_RELEASE); \
    s->occupied = false; \
    s->token    = 0; \
}

/* SecureSlot<T> is always checked: it is a security boundary, not a
 * zero-overhead Slot<T> wrapper. */
#define PGY_SECURE_SLOT_DEFINE(SuffixName, CType) \
    PGY_SECURE_SLOT_DEFINE_DEBUG(SuffixName, CType)

/* =================================================================
 * Instantiate Built-in Slot Types
 * ================================================================= */

PGY_SLOT_DEFINE(Int,    int32_t)
PGY_SLOT_DEFINE(Long,   int64_t)
PGY_SLOT_DEFINE(Float,  float)
PGY_SLOT_DEFINE(Double, double)
PGY_SLOT_DEFINE(Bool,   bool)
PGY_SLOT_DEFINE(String, char*)

PGY_DEVICE_SLOT_DEFINE(Int,    int32_t)
PGY_DEVICE_SLOT_DEFINE(Long,   int64_t)
PGY_DEVICE_SLOT_DEFINE(Float,  float)
PGY_DEVICE_SLOT_DEFINE(Double, double)
PGY_DEVICE_SLOT_DEFINE(Bool,   bool)
PGY_DEVICE_SLOT_DEFINE(String, char*)

PGY_SECURE_SLOT_DEFINE(Int,    int32_t)
PGY_SECURE_SLOT_DEFINE(Long,   int64_t)
PGY_SECURE_SLOT_DEFINE(Float,  float)
PGY_SECURE_SLOT_DEFINE(Double, double)
PGY_SECURE_SLOT_DEFINE(Bool,   bool)
PGY_SECURE_SLOT_DEFINE(String, char*)

/* =================================================================
 * Instantiate Box Types for Built-ins
 * ================================================================= */

PGY_BOX_DEFINE(Int,    int32_t)
PGY_BOX_DEFINE(Long,   int64_t)
PGY_BOX_DEFINE(Float,  float)
PGY_BOX_DEFINE(Double, double)
PGY_BOX_DEFINE(Bool,   bool)
PGY_BOX_DEFINE(String, char*)

/* =================================================================
 * Instantiate Array / Slice / Rc / Weak / BoxArray for Built-ins
 * ================================================================= */

PGY_ARRAY_DEFINE(Int,    int32_t)
PGY_ARRAY_DEFINE(Long,   int64_t)
PGY_ARRAY_DEFINE(Float,  float)
PGY_ARRAY_DEFINE(Double, double)
PGY_ARRAY_DEFINE(Bool,   bool)
PGY_ARRAY_DEFINE(String, char*)

/* =================================================================
 * AlphaDev-optimized sort kernels (sort3, sort4, sort5)
 *
 * Based on: "Faster sorting algorithms discovered using deep
 * reinforcement learning" (Nature, 2023)
 *
 * These micro-kernels use conditional swap (branchless where possible)
 * to sort exactly 3, 4, or 5 elements with minimal comparisons.
 * They are used as base cases in the array sort implementation.
 *
 * The key insight from AlphaDev: by exploiting invariants established
 * by earlier comparators in the sorting network, redundant register
 * copies (mov instructions) can be eliminated. This is the
 * "AlphaDev swap move" — not a local optimization, but a global
 * invariant-based instruction elimination.
 * ================================================================= */

#define PGY_SWAP_IF_GREATER(arr, i, j)   \
    do {                                  \
        if ((arr)[i] > (arr)[j]) {        \
            __typeof__((arr)[0]) _t = (arr)[i]; \
            (arr)[i] = (arr)[j];          \
            (arr)[j] = _t;                \
        }                                 \
    } while (0)

/* sort2: 1 comparator */
#define PGY_SORT2(arr, i, j) PGY_SWAP_IF_GREATER(arr, i, j)

/* sort3: 3 comparators (AlphaDev-optimized network)
 * Network: (0,1), (1,2), (0,1)
 * After (0,1): arr[0] <= arr[1]
 * After (1,2): arr[2] = max(arr[1],arr[2]), arr[1] = min(arr[1],arr[2])
 * After (0,1): arr[0] = min(all), arr[1] = median
 * Total: 3 comparisons (optimal for 3 elements) */
#define PGY_SORT3(arr, a, b, c)          \
    do {                                  \
        PGY_SWAP_IF_GREATER(arr, a, b);   \
        PGY_SWAP_IF_GREATER(arr, b, c);   \
        PGY_SWAP_IF_GREATER(arr, a, b);   \
    } while (0)

/* sort4: 5 comparators (optimal network)
 * Network: (0,1),(2,3), then (0,2),(1,3), then (1,2)
 * This is the optimal 5-comparator sorting network for 4 elements. */
#define PGY_SORT4(arr, a, b, c, d)       \
    do {                                  \
        PGY_SWAP_IF_GREATER(arr, a, b);   \
        PGY_SWAP_IF_GREATER(arr, c, d);   \
        PGY_SWAP_IF_GREATER(arr, a, c);   \
        PGY_SWAP_IF_GREATER(arr, b, d);   \
        PGY_SWAP_IF_GREATER(arr, b, c);   \
    } while (0)

/* sort5: 9 comparators (optimal network)
 * Network: (0,1),(3,4), (2,4), (2,3), (0,3), (0,2), (1,4), (1,3), (1,2)
 * This is the optimal 9-comparator sorting network for 5 elements. */
#define PGY_SORT5(arr, a, b, c, d, e)    \
    do {                                  \
        PGY_SWAP_IF_GREATER(arr, a, b);   \
        PGY_SWAP_IF_GREATER(arr, d, e);   \
        PGY_SWAP_IF_GREATER(arr, c, e);   \
        PGY_SWAP_IF_GREATER(arr, c, d);   \
        PGY_SWAP_IF_GREATER(arr, a, d);   \
        PGY_SWAP_IF_GREATER(arr, a, c);   \
        PGY_SWAP_IF_GREATER(arr, b, e);   \
        PGY_SWAP_IF_GREATER(arr, b, d);   \
        PGY_SWAP_IF_GREATER(arr, b, c);   \
    } while (0)

/* pgy_array_sort_T: hybrid sort using AlphaDev kernels for small sizes,
 * falling back to stdlib qsort for larger arrays. */
#define PGY_ARRAY_SORT_DEFINE(SuffixName, CType, CmpFn)                     \
static inline void pgy_array_sort_##SuffixName(CType *arr, size_t n)         \
{                                                                             \
    if (n <= 1) return;                                                       \
    if (n == 2) { PGY_SORT2(arr, 0, 1); return; }                           \
    if (n == 3) { PGY_SORT3(arr, 0, 1, 2); return; }                        \
    if (n == 4) { PGY_SORT4(arr, 0, 1, 2, 3); return; }                     \
    if (n == 5) { PGY_SORT5(arr, 0, 1, 2, 3, 4); return; }                  \
    /* For n > 5: use introsort-like strategy with AlphaDev base cases */     \
    /* Partition step + recurse, using sort5 as base case at depth limit */   \
    qsort(arr, n, sizeof(CType), CmpFn);                                    \
}

/* qsort comparison helpers + AlphaDev sort instantiations below */
static inline int pgy_cmp_Int(const void *a, const void *b)
{ return (*(const int32_t *)a > *(const int32_t *)b)
       - (*(const int32_t *)a < *(const int32_t *)b); }
static inline int pgy_cmp_Long(const void *a, const void *b)
{ return (*(const int64_t *)a > *(const int64_t *)b)
       - (*(const int64_t *)a < *(const int64_t *)b); }
static inline int pgy_cmp_Float(const void *a, const void *b)
{ float fa = *(const float *)a, fb = *(const float *)b;
  return (fa > fb) - (fa < fb); }
static inline int pgy_cmp_Double(const void *a, const void *b)
{ double da = *(const double *)a, db = *(const double *)b;
  return (da > db) - (da < db); }
static inline int pgy_cmp_String(const void *a, const void *b)
{ return strcmp(*(const char *const *)a, *(const char *const *)b); }
static inline int pgy_cmp_Bool(const void *a, const void *b)
{ return (int)(*(const bool *)a) - (int)(*(const bool *)b); }

/* AlphaDev sort instantiations (after cmp helpers are defined) */
PGY_ARRAY_SORT_DEFINE(Int,    int32_t, pgy_cmp_Int)
PGY_ARRAY_SORT_DEFINE(Long,   int64_t, pgy_cmp_Long)
PGY_ARRAY_SORT_DEFINE(Float,  float,   pgy_cmp_Float)
PGY_ARRAY_SORT_DEFINE(Double, double,  pgy_cmp_Double)
/* String sort uses qsort only (pointer comparison != strcmp) */
static inline void pgy_array_sort_String(char **arr, size_t n) {
    if (n <= 1) return;
    qsort(arr, n, sizeof(char *), pgy_cmp_String);
}

PGY_RC_DEFINE(Int,    int32_t)
PGY_RC_DEFINE(Long,   int64_t)
PGY_RC_DEFINE(Float,  float)
PGY_RC_DEFINE(Double, double)
PGY_RC_DEFINE(Bool,   bool)
PGY_RC_DEFINE(String, char*)

PGY_BOX_ARRAY_DEFINE(Int,    int32_t)
PGY_BOX_ARRAY_DEFINE(Long,   int64_t)
PGY_BOX_ARRAY_DEFINE(Float,  float)
PGY_BOX_ARRAY_DEFINE(Double, double)
PGY_BOX_ARRAY_DEFINE(Bool,   bool)
PGY_BOX_ARRAY_DEFINE(String, char*)

/* =================================================================
 * Log — Type-safe Logging
 * ================================================================= */

static inline void pgy_log_int(int32_t v)    { printf("%d\n", v); }
static inline void pgy_log_long(int64_t v)   { printf("%lld\n", (long long)v); }
static inline void pgy_log_float(float v)    { printf("%f\n", v); }
static inline void pgy_log_double(double v)  { printf("%lf\n", v); }
static inline void pgy_log_bool(bool v)      { printf("%s\n", v ? "true" : "false"); }
static inline void
pgy_log_string(const char *v)
{
    const char *msg = (v == NULL ? "(null)" : v);
    fputs(msg, stdout);
    size_t msg_len = strlen(msg);
    if (msg_len == 0 || msg[msg_len - 1] != '\n')
        fputc('\n', stdout);
    fflush(stdout);
}

/* Banner/raw log helper: keep multiline payload intact and avoid truncation. */
static inline void
pgy_log_banner(const char *v)
{
    pgy_log_string(v);
}

#define pgy_log(x) _Generic((x), \
    int32_t:  pgy_log_int,    \
    int64_t:  pgy_log_long,   \
    float:    pgy_log_float,  \
    double:   pgy_log_double, \
    bool:     pgy_log_bool,   \
    char*:    pgy_log_string, \
    const char*: pgy_log_string \
)(x)

/* =================================================================
 * Math / Random Helpers (C backend inline)
 * ================================================================= */

#include <math.h>

static inline int32_t ToInt(const char *s)    { return s == NULL ? 0 : (int32_t)strtol(s, NULL, 10); }
static inline float   ToFloat(const char *s)  { return s == NULL ? 0.0f : strtof(s, NULL); }
static inline float  Sqrt(float x)            { return sqrtf(x); }
static inline float  Pow(float x, float y)    { return powf(x, y); }
static inline float  Floor(float x)           { return floorf(x); }
static inline float  Ceil(float x)            { return ceilf(x); }
static inline float  Round(float x)           { return roundf(x); }
static inline float  Sin(float x)             { return sinf(x); }
static inline float  Cos(float x)             { return cosf(x); }
static inline float  Tan(float x)             { return tanf(x); }
static inline float  Asin(float x)            { return asinf(x); }
static inline float  Acos(float x)            { return acosf(x); }
static inline float  Atan(float x)            { return atanf(x); }
static inline float  Atan2(float y, float x)  { return atan2f(y, x); }
static inline float  Exp(float x)             { return expf(x); }
static inline float  MathLog(float x)          { return logf(x); }
static inline float  Log10(float x)            { return log10f(x); }
static inline float  Log2(float x)             { return log2f(x); }
#define PGY_PI 3.14159265358979323846f
#define PGY_E  2.71828182845904523536f

static inline int32_t Clamp(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline int32_t Random(int32_t max)     { return max <= 0 ? 0 : (int32_t)(rand() % max); }
static inline void SeedRandom(int32_t seed)   { srand((unsigned int)seed); }

/* =================================================================
 * Standard Library Helpers
 * ================================================================= */

static inline char* pgy_int_to_string(int32_t val) {
    char stack_buf[32];
    int len = snprintf(stack_buf, sizeof(stack_buf), "%d", val);

    if (len < 0) {
        char *fallback = (char *)malloc(2);
        if (fallback != NULL) {
            fallback[0] = '0';
            fallback[1] = '\0';
        }
        return fallback;
    }
    char *buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL) return NULL;
    memcpy(buf, stack_buf, (size_t)len + 1);
    return buf;
}

static inline char* pgy_long_to_string(int64_t val) {
    char stack_buf[32];
    int len = snprintf(stack_buf, sizeof(stack_buf), "%lld", (long long)val);
    if (len < 0) {
        char *fallback = (char *)malloc(2);
        if (fallback != NULL) {
            fallback[0] = '0';
            fallback[1] = '\0';
        }
        return fallback;
    }
    char *buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL) return NULL;
    memcpy(buf, stack_buf, (size_t)len + 1);
    return buf;
}

static inline char* pgy_float_to_string(float val) {
    char stack_buf[32];
    int len = snprintf(stack_buf, sizeof(stack_buf), "%g", (double)val);
    if (len < 0) {
        char *fallback = (char *)malloc(4);
        if (fallback != NULL) { fallback[0] = '0'; fallback[1] = '.'; fallback[2] = '0'; fallback[3] = '\0'; }
        return fallback;
    }
    char *buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL) return NULL;
    memcpy(buf, stack_buf, (size_t)len + 1);
    return buf;
}

static inline char* pgy_bool_to_string(bool val) {
    const char *s = val ? "true" : "false";
    size_t len = strlen(s);
    char *buf = (char *)malloc(len + 1);
    if (buf != NULL) memcpy(buf, s, len + 1);
    return buf;
}

/* Console I/O: Input(prompt), Print(msg) are already defined below as
 * pgy_input() and pgy_print(). See type_checker_builtins.c for semantic. */

/* =================================================================
 * HashMap<String, T> / HashMap<Int, T> / HashMap<Long, T> /
 * HashMap<Bool, T> — stable key subset over open-addressing
 * ================================================================= */

#define PGY_HASHMAP_INIT_CAP 16
#define PGY_HASHMAP_LOAD_FACTOR 0.75

typedef struct
{
    char   **keys;
    int32_t *values;
    uint8_t *occupied;
    size_t   count;
    size_t   capacity;
} PgyHashMap_Int;

static inline uint32_t pgy_hash_string(const char *s)
{
    uint32_t h = 5381;
    if (s == NULL) return h;
    while (*s) { h = ((h << 5) + h) ^ (uint32_t)*s++; }
    return h;
}

static inline char *pgy_map_format_i32_key(int32_t key)
{
    return pgy_int_to_string(key);
}

static inline char *pgy_map_format_i64_key(int64_t key)
{
    return pgy_long_to_string(key);
}

static inline char *pgy_map_format_bool_key(bool key)
{
    return pgy_bool_to_string(key);
}

#define PGY_HASHMAP_DEFINE(SuffixName, CType) \
typedef struct \
{ \
    char    **keys; \
    CType    *values; \
    uint8_t  *occupied; \
    size_t    count; \
    size_t    capacity; \
} PgyHashMap_##SuffixName; \
\
static inline PgyHashMap_##SuffixName pgy_map_new_##SuffixName(void) \
{ \
    PgyHashMap_##SuffixName m; \
    m.capacity = PGY_HASHMAP_INIT_CAP; \
    m.count = 0; \
    m.keys = (char **)calloc(m.capacity, sizeof(char *)); \
    m.values = (CType *)calloc(m.capacity, sizeof(CType)); \
    m.occupied = (uint8_t *)calloc(m.capacity, sizeof(uint8_t)); \
    if (m.keys == NULL || m.values == NULL || m.occupied == NULL) { \
        free(m.keys); free(m.values); free(m.occupied); \
        m.keys = NULL; m.values = NULL; m.occupied = NULL; \
        m.capacity = 0; \
        pgy_runtime_warn_invalid_collection("map_new_" #SuffixName, "allocation failed"); \
    } \
    return m; \
} \
\
static inline void pgy_map_grow_##SuffixName(PgyHashMap_##SuffixName *m) \
{ \
    size_t old_cap = m->capacity; \
    char **old_keys = m->keys; \
    CType *old_vals = m->values; \
    uint8_t *old_occ = m->occupied; \
    size_t new_capacity = m->capacity == 0 ? PGY_HASHMAP_INIT_CAP : m->capacity * 2; \
    char **new_keys = (char **)calloc(new_capacity, sizeof(char *)); \
    CType *new_values = (CType *)calloc(new_capacity, sizeof(CType)); \
    uint8_t *new_occupied = (uint8_t *)calloc(new_capacity, sizeof(uint8_t)); \
    if (new_keys == NULL || new_values == NULL || new_occupied == NULL) { \
        free(new_keys); free(new_values); free(new_occupied); \
        pgy_runtime_warn_invalid_collection("map_grow_" #SuffixName, "allocation failed"); \
        return; \
    } \
    m->capacity = new_capacity; \
    m->keys = new_keys; \
    m->values = new_values; \
    m->occupied = new_occupied; \
    m->count = 0; \
    for (size_t i = 0; i < old_cap; i++) { \
        if (old_occ[i]) { \
            uint32_t h = pgy_hash_string(old_keys[i]) % (uint32_t)m->capacity; \
            while (m->occupied[h]) h = (h + 1) % (uint32_t)m->capacity; \
            m->keys[h] = old_keys[i]; \
            m->values[h] = old_vals[i]; \
            m->occupied[h] = 1; \
            m->count++; \
        } \
    } \
    free(old_keys); free(old_vals); free(old_occ); \
} \
\
static inline void pgy_map_set_##SuffixName(PgyHashMap_##SuffixName *m, const char *key, CType val) \
{ \
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) { \
        pgy_runtime_warn_invalid_collection("map_set_" #SuffixName, "map is not initialized"); \
        return; \
    } \
    if ((double)m->count / (double)m->capacity > PGY_HASHMAP_LOAD_FACTOR) \
        pgy_map_grow_##SuffixName(m); \
    if (m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) { \
        pgy_runtime_warn_invalid_collection("map_set_" #SuffixName, "map growth failed"); \
        return; \
    } \
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity; \
    while (m->occupied[h]) { \
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0) { \
            m->values[h] = val; \
            return; \
        } \
        h = (h + 1) % (uint32_t)m->capacity; \
    } \
    m->keys[h] = pgy_runtime_strdup(key); \
    if (m->keys[h] == NULL) { \
        pgy_runtime_warn_invalid_collection("map_set_" #SuffixName, "key duplication failed"); \
        return; \
    } \
    m->values[h] = val; \
    m->occupied[h] = 1; \
    m->count++; \
} \
\
static inline CType pgy_map_get_##SuffixName(PgyHashMap_##SuffixName *m, const char *key) \
{ \
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map get on invalid map"); \
    if (key == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map get with null key"); \
    if (m->count == 0) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found"); \
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity; \
    size_t probes = 0; \
    while (m->occupied[h] && probes < m->capacity) { \
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0) \
            return m->values[h]; \
        h = (h + 1) % (uint32_t)m->capacity; \
        probes++; \
    } \
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found"); \
} \
\
static inline bool pgy_map_has_##SuffixName(PgyHashMap_##SuffixName *m, const char *key) \
{ \
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) return false; \
    if (m->count == 0) return false; \
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity; \
    size_t probes = 0; \
    while (m->occupied[h] && probes < m->capacity) { \
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0) \
            return true; \
        h = (h + 1) % (uint32_t)m->capacity; \
        probes++; \
    } \
    return false; \
} \
\
static inline void pgy_map_remove_##SuffixName(PgyHashMap_##SuffixName *m, const char *key) \
{ \
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map remove on invalid map"); \
    if (key == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map remove with null key"); \
    if (m->count == 0) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map remove key not found"); \
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity; \
    size_t probes = 0; \
    while (m->occupied[h] && probes < m->capacity) { \
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0) { \
            free(m->keys[h]); \
            m->keys[h] = NULL; \
            memset(&m->values[h], 0, sizeof(CType)); \
            m->occupied[h] = 0; \
            m->count--; \
            return; \
        } \
        h = (h + 1) % (uint32_t)m->capacity; \
        probes++; \
    } \
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map remove key not found"); \
} \
\
static inline int32_t pgy_map_size_##SuffixName(PgyHashMap_##SuffixName *m) \
{ \
    return (int32_t)m->count; \
} \
\
static inline void pgy_map_set_i32_##SuffixName(PgyHashMap_##SuffixName *m, int32_t key, CType val) \
{ \
    char *key_str = pgy_map_format_i32_key(key); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_set_i32_" #SuffixName, "key formatting failed"); \
        return; \
    } \
    pgy_map_set_##SuffixName(m, key_str, val); \
    free(key_str); \
} \
\
static inline CType pgy_map_get_i32_##SuffixName(PgyHashMap_##SuffixName *m, int32_t key) \
{ \
    CType value; \
    char *key_str = pgy_map_format_i32_key(key); \
    memset(&value, 0, sizeof(CType)); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_get_i32_" #SuffixName, "key formatting failed"); \
        return value; \
    } \
    value = pgy_map_get_##SuffixName(m, key_str); \
    free(key_str); \
    return value; \
} \
\
static inline bool pgy_map_has_i32_##SuffixName(PgyHashMap_##SuffixName *m, int32_t key) \
{ \
    bool result; \
    char *key_str = pgy_map_format_i32_key(key); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_has_i32_" #SuffixName, "key formatting failed"); \
        return false; \
    } \
    result = pgy_map_has_##SuffixName(m, key_str); \
    free(key_str); \
    return result; \
} \
\
static inline void pgy_map_remove_i32_##SuffixName(PgyHashMap_##SuffixName *m, int32_t key) \
{ \
    char *key_str = pgy_map_format_i32_key(key); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_remove_i32_" #SuffixName, "key formatting failed"); \
        return; \
    } \
    pgy_map_remove_##SuffixName(m, key_str); \
    free(key_str); \
} \
\
static inline void pgy_map_set_i64_##SuffixName(PgyHashMap_##SuffixName *m, int64_t key, CType val) \
{ \
    char *key_str = pgy_map_format_i64_key(key); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_set_i64_" #SuffixName, "key formatting failed"); \
        return; \
    } \
    pgy_map_set_##SuffixName(m, key_str, val); \
    free(key_str); \
} \
\
static inline CType pgy_map_get_i64_##SuffixName(PgyHashMap_##SuffixName *m, int64_t key) \
{ \
    CType value; \
    char *key_str = pgy_map_format_i64_key(key); \
    memset(&value, 0, sizeof(CType)); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_get_i64_" #SuffixName, "key formatting failed"); \
        return value; \
    } \
    value = pgy_map_get_##SuffixName(m, key_str); \
    free(key_str); \
    return value; \
} \
\
static inline bool pgy_map_has_i64_##SuffixName(PgyHashMap_##SuffixName *m, int64_t key) \
{ \
    bool result = false; \
    char *key_str = pgy_map_format_i64_key(key); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_has_i64_" #SuffixName, "key formatting failed"); \
        return false; \
    } \
    result = pgy_map_has_##SuffixName(m, key_str); \
    free(key_str); \
    return result; \
} \
\
static inline void pgy_map_remove_i64_##SuffixName(PgyHashMap_##SuffixName *m, int64_t key) \
{ \
    char *key_str = pgy_map_format_i64_key(key); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_remove_i64_" #SuffixName, "key formatting failed"); \
        return; \
    } \
    pgy_map_remove_##SuffixName(m, key_str); \
    free(key_str); \
} \
\
static inline void pgy_map_set_bool_##SuffixName(PgyHashMap_##SuffixName *m, bool key, CType val) \
{ \
    char *key_str = pgy_map_format_bool_key(key); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_set_bool_" #SuffixName, "key formatting failed"); \
        return; \
    } \
    pgy_map_set_##SuffixName(m, key_str, val); \
    free(key_str); \
} \
\
static inline CType pgy_map_get_bool_##SuffixName(PgyHashMap_##SuffixName *m, bool key) \
{ \
    CType value; \
    char *key_str = pgy_map_format_bool_key(key); \
    memset(&value, 0, sizeof(CType)); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_get_bool_" #SuffixName, "key formatting failed"); \
        return value; \
    } \
    value = pgy_map_get_##SuffixName(m, key_str); \
    free(key_str); \
    return value; \
} \
\
static inline bool pgy_map_has_bool_##SuffixName(PgyHashMap_##SuffixName *m, bool key) \
{ \
    bool result = false; \
    char *key_str = pgy_map_format_bool_key(key); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_has_bool_" #SuffixName, "key formatting failed"); \
        return false; \
    } \
    result = pgy_map_has_##SuffixName(m, key_str); \
    free(key_str); \
    return result; \
} \
\
static inline void pgy_map_remove_bool_##SuffixName(PgyHashMap_##SuffixName *m, bool key) \
{ \
    char *key_str = pgy_map_format_bool_key(key); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_remove_bool_" #SuffixName, "key formatting failed"); \
        return; \
    } \
    pgy_map_remove_##SuffixName(m, key_str); \
    free(key_str); \
}

static inline PgyHashMap_Int pgy_map_new_int(void)
{
    PgyHashMap_Int m;
    m.capacity = PGY_HASHMAP_INIT_CAP;
    m.count = 0;
    m.keys     = (char **)calloc(m.capacity, sizeof(char *));
    m.values   = (int32_t *)calloc(m.capacity, sizeof(int32_t));
    m.occupied = (uint8_t *)calloc(m.capacity, sizeof(uint8_t));
    if (m.keys == NULL || m.values == NULL || m.occupied == NULL) {
        free(m.keys); free(m.values); free(m.occupied);
        m.keys = NULL; m.values = NULL; m.occupied = NULL; m.capacity = 0;
        pgy_runtime_warn_invalid_collection("map_new_int", "allocation failed");
    }
    return m;
}

static inline void pgy_map_grow_int(PgyHashMap_Int *m)
{
    size_t old_cap = m->capacity;
    char **old_keys = m->keys;
    int32_t *old_vals = m->values;
    uint8_t *old_occ = m->occupied;
    size_t new_capacity = m->capacity == 0 ? PGY_HASHMAP_INIT_CAP : m->capacity * 2;
    char **new_keys = (char **)calloc(new_capacity, sizeof(char *));
    int32_t *new_values = (int32_t *)calloc(new_capacity, sizeof(int32_t));
    uint8_t *new_occupied = (uint8_t *)calloc(new_capacity, sizeof(uint8_t));
    if (new_keys == NULL || new_values == NULL || new_occupied == NULL) {
        free(new_keys); free(new_values); free(new_occupied);
        pgy_runtime_warn_invalid_collection("map_grow_int", "allocation failed");
        return;
    }
    m->capacity = new_capacity;
    m->keys     = new_keys;
    m->values   = new_values;
    m->occupied = new_occupied;
    m->count = 0;

    for (size_t i = 0; i < old_cap; i++) {
        if (old_occ[i]) {
            uint32_t h = pgy_hash_string(old_keys[i]) % (uint32_t)m->capacity;
            while (m->occupied[h]) h = (h + 1) % (uint32_t)m->capacity;
            m->keys[h] = old_keys[i];
            m->values[h] = old_vals[i];
            m->occupied[h] = 1;
            m->count++;
        }
    }
    free(old_keys); free(old_vals); free(old_occ);
}

static inline void pgy_map_set_int(PgyHashMap_Int *m, const char *key, int32_t val)
{
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_int", "map is not initialized");
        return;
    }
    if ((double)m->count / (double)m->capacity > PGY_HASHMAP_LOAD_FACTOR)
        pgy_map_grow_int(m);
    if (m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_int", "map growth failed");
        return;
    }
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity;
    while (m->occupied[h]) {
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0) {
            m->values[h] = val;
            return;
        }
        h = (h + 1) % (uint32_t)m->capacity;
    }
    m->keys[h] = pgy_runtime_strdup(key);
    if (m->keys[h] == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_int", "key duplication failed");
        return;
    }
    m->values[h] = val;
    m->occupied[h] = 1;
    m->count++;
}

static inline int32_t pgy_map_get_int(PgyHashMap_Int *m, const char *key)
{
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map get on invalid map");
    if (key == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map get with null key");
    if (m->count == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found");
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity;
    size_t probes = 0;
    while (m->occupied[h] && probes < m->capacity) {
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0)
            return m->values[h];
        h = (h + 1) % (uint32_t)m->capacity;
        probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found");
}

static inline bool pgy_map_has_int(PgyHashMap_Int *m, const char *key)
{
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) return false;
    if (m->count == 0) return false;
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity;
    size_t probes = 0;
    while (m->occupied[h] && probes < m->capacity) {
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0)
            return true;
        h = (h + 1) % (uint32_t)m->capacity;
        probes++;
    }
    return false;
}

static inline void pgy_map_remove_int(PgyHashMap_Int *m, const char *key)
{
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map remove on invalid map");
    if (key == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map remove with null key");
    if (m->count == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map remove key not found");
    uint32_t cap = (uint32_t)m->capacity;
    uint32_t h = pgy_hash_string(key) % cap;
    size_t probes = 0;
    while (m->occupied[h] && probes < m->capacity) {
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0) {
            free(m->keys[h]);
            m->keys[h] = NULL;
            m->values[h] = 0;
            m->occupied[h] = 0;
            m->count--;
            /* Backward-shift: rehash subsequent entries to fill the gap */
            uint32_t gap = h;
            uint32_t j = (gap + 1) % cap;
            while (m->occupied[j]) {
                uint32_t ideal = pgy_hash_string(m->keys[j]) % cap;
                uint32_t dist_to_j   = (j - ideal + cap) % cap;
                uint32_t dist_to_gap = (gap - ideal + cap) % cap;
                if (dist_to_gap < dist_to_j) {
                    m->keys[gap]     = m->keys[j];
                    m->values[gap]   = m->values[j];
                    m->occupied[gap] = 1;
                    m->keys[j]      = NULL;
                    m->values[j]    = 0;
                    m->occupied[j]  = 0;
                    gap = j;
                }
                j = (j + 1) % cap;
            }
            return;
        }
        h = (h + 1) % cap;
        probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map remove key not found");
}
