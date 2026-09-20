#ifndef PERGYRA_HASHMAP_KEY_STORAGE_KIND_H
#define PERGYRA_HASHMAP_KEY_STORAGE_KIND_H

/* Physical HashMap key storage.  A constructor fixes this fact before any
 * operation may inspect storage. */
typedef enum PgyHashMapKeyStorageKind {
    PGY_HASHMAP_KEY_STORAGE_INVALID = 0,
    PGY_HASHMAP_KEY_STORAGE_STRING = 1,
    PGY_HASHMAP_KEY_STORAGE_I32 = 2,
    PGY_HASHMAP_KEY_STORAGE_I64 = 3,
    PGY_HASHMAP_KEY_STORAGE_BOOL = 4
} PgyHashMapKeyStorageKind;

#endif /* PERGYRA_HASHMAP_KEY_STORAGE_KIND_H */
