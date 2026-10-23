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

/* A map's storage copied slot by slot into new arrays of the same capacity:
 * the same marker in every slot, a String key (and a String value when
 * `string_values`) duplicated in each live slot, zero in the others. Nothing
 * is rehashed, so MapKeys gives the same order on both maps. False, with
 * nothing left allocated, when an allocation fails. */
static inline bool
pgy_hashmap_clone_slots(void **keys_out, void **values_out, uint8_t **occupied_out,
                        const void *keys, const void *values, const uint8_t *occupied,
                        size_t capacity, int32_t key_storage_kind, size_t value_size,
                        bool string_values)
{
    size_t key_size = key_storage_kind == PGY_HASHMAP_KEY_STORAGE_STRING ? sizeof(char *)
        : key_storage_kind == PGY_HASHMAP_KEY_STORAGE_I32 ? sizeof(int32_t)
        : key_storage_kind == PGY_HASHMAP_KEY_STORAGE_I64 ? sizeof(int64_t) : sizeof(bool);
    char *k = (char *)PGY_HASHMAP_CALLOC(capacity, key_size);
    char *v = (char *)PGY_HASHMAP_CALLOC(capacity, value_size);
    uint8_t *o = (uint8_t *)PGY_HASHMAP_CALLOC(capacity, sizeof(uint8_t));
    bool ok = k != NULL && v != NULL && o != NULL;
    for (size_t i = 0; ok && i < capacity; i++) {
        o[i] = occupied[i];
        if (occupied[i] != PGY_HASHMAP_LIVE) continue;
        if (key_storage_kind == PGY_HASHMAP_KEY_STORAGE_STRING) {
            const char *key = ((char *const *)keys)[i];
            ok = key == NULL || (((char **)k)[i] = pgy_runtime_strdup(key)) != NULL;
        } else {
            memcpy(k + i * key_size, (const char *)keys + i * key_size, key_size);
        }
        if (ok && string_values) {
            const char *value = ((char *const *)values)[i];
            ok = value == NULL || (((char **)v)[i] = pgy_runtime_strdup(value)) != NULL;
        } else if (ok) {
            memcpy(v + i * value_size, (const char *)values + i * value_size, value_size);
        }
    }
    if (!ok) {
        for (size_t i = 0; o != NULL && i < capacity; i++) {
            if (k != NULL && key_storage_kind == PGY_HASHMAP_KEY_STORAGE_STRING) free(((char **)k)[i]);
            if (v != NULL && string_values) free(((char **)v)[i]);
        }
        free(k); free(v); free(o);
        return false;
    }
    *keys_out = k;
    *values_out = v;
    *occupied_out = o;
    return true;
}

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
