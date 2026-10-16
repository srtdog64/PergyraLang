#include "pgy_runtime_linkage.h"
/* =================================================================
 * HashMap<String, T> / HashMap<Int, T> / HashMap<Long, T> /
 * HashMap<Bool, T> -- stable key subset over open-addressing
 * ================================================================= */

/* Growable runtime storage is not a synchronization boundary.
 * The semantic layer must reject raw Array/Slice/List/Queue/Set/HashMap
 * transport across parallel/async/worker boundaries unless an explicit copy or
 * pinned read-only view owner is used.
 */

#define PGY_HASHMAP_INIT_CAP 16
#define PGY_HASHMAP_LOAD_FACTOR 0.75
#define PGY_HASHMAP_EMPTY 0u
#define PGY_HASHMAP_LIVE 1u
#define PGY_HASHMAP_DELETED 2u

#include "pgy_runtime_hashmap_key_storage_owner.h"

#define PGY_HASHMAP_STRING_KEYS(map) ((char **)((map)->keys))
#define PGY_HASHMAP_I32_KEYS(map) ((int32_t *)((map)->keys))
#define PGY_HASHMAP_I64_KEYS(map) ((int64_t *)((map)->keys))
#define PGY_HASHMAP_BOOL_KEYS(map) ((bool *)((map)->keys))

#ifndef PGY_RUNTIME_HASHMAP_CAPACITY_FITS
#define PGY_RUNTIME_HASHMAP_CAPACITY_FITS(capacity, CType) \
    ((capacity) != 0 \
        && (capacity) <= (size_t)INT32_MAX \
        && (capacity) <= SIZE_MAX / sizeof(char *) \
        && (capacity) <= SIZE_MAX / sizeof(CType) \
        && (capacity) <= SIZE_MAX / sizeof(uint8_t))
#endif

#ifndef PGY_RUNTIME_HASHMAP_IS_INITIALIZED
#define PGY_RUNTIME_HASHMAP_IS_INITIALIZED(map, CType) \
    ((map) != NULL \
        && PGY_RUNTIME_HASHMAP_CAPACITY_FITS((map)->capacity, CType) \
        && (map)->keys != NULL \
        && (map)->values != NULL \
        && (map)->occupied != NULL \
        && ((map)->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_STRING \
            || (map)->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_I32 \
            || (map)->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_I64 \
            || (map)->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_BOOL))
#endif

typedef struct
{
    void    *keys;
    int32_t *values;
    uint8_t *occupied;
    size_t   count;
    size_t   capacity;
    size_t   deleted_count;
    int32_t  key_storage_kind;
} PgyHashMap_Int;

PGY_RT_DECL uint32_t pgy_hash_string(const char *s)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    uint32_t h = 5381;
    if (s == NULL) return h;
    while (*s) { h = ((h << 5) + h) ^ (uint32_t)*s++; }
    return h;
}
#else
;
#endif
