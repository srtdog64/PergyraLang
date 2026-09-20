#ifndef PGY_RUNTIME_HASHMAP_KEY_STORAGE_OWNER_H
#define PGY_RUNTIME_HASHMAP_KEY_STORAGE_OWNER_H

#include "../common/hashmap_key_storage_kind.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef PGY_HASHMAP_CALLOC
#define PGY_HASHMAP_CALLOC(count, size) calloc((count), (size))
#endif

/* Integer hashing is defined in unsigned space so INT32_MIN and every other
 * source Int have a stable, overflow-free key identity. */
static inline uint32_t
pgy_hashmap_hash_i32(int32_t key)
{
    uint32_t value = (uint32_t)key;
    value ^= value >> 16;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16;
    return value;
}

/* Fold both halves only after an unsigned 64-bit avalanche.  Keys that share
 * their low 32 bits therefore do not collapse into the old truncated lane. */
static inline uint32_t
pgy_hashmap_hash_i64(int64_t key)
{
    uint64_t value = (uint64_t)key;
    value ^= value >> 30;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31;
    return (uint32_t)value ^ (uint32_t)(value >> 32);
}

static inline uint32_t
pgy_hashmap_hash_bool(bool key)
{
    return key ? UINT32_C(0x9e3779b9) : UINT32_C(0x85ebca6b);
}

static inline size_t
pgy_hashmap_key_storage_size(PgyHashMapKeyStorageKind kind)
{
    if (kind == PGY_HASHMAP_KEY_STORAGE_STRING)
        return sizeof(char *);
    if (kind == PGY_HASHMAP_KEY_STORAGE_I32)
        return sizeof(int32_t);
    if (kind == PGY_HASHMAP_KEY_STORAGE_I64)
        return sizeof(int64_t);
    if (kind == PGY_HASHMAP_KEY_STORAGE_BOOL)
        return sizeof(bool);
    return 0;
}

#endif /* PGY_RUNTIME_HASHMAP_KEY_STORAGE_OWNER_H */
