/* =================================================================
 * Set raw (type-erased) export functions for LLVM linking
 *
 * Generic hash set using open-addressing with FNV-1a hash on raw bytes.
 * Works for raw byte element types (Int, Long, Bool, Float, structs) without
 * requiring string conversion. String uses the dedicated string add/has/remove
 * exports below so pointer-sized non-string values are never treated as
 * `char *`.
 *
 * Layout matches PgySet_Generic:
 *   void    *data       - element storage (elem_size * capacity)
 *   uint8_t *occupied   — slot occupancy flags
 *   size_t   count
 *   size_t   capacity
 * ================================================================= */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    void    *data;
    uint8_t *occupied;
    size_t   count;
    size_t   capacity;
} PgySetRaw;

#define PGY_SET_RAW_EMPTY 0u
#define PGY_SET_RAW_LIVE 1u
#define PGY_SET_RAW_DELETED 2u

static bool
pgy_set_raw_shape_fits(size_t capacity, size_t elem_size)
{
    return capacity != 0
        && capacity <= (size_t)INT32_MAX
        && elem_size != 0
        && elem_size <= SIZE_MAX / capacity;
}

static uint32_t
pgy_hash_bytes(const void *ptr, size_t len)
{
    const uint8_t *p = (const uint8_t *)ptr;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

static bool
pgy_set_raw_elem_eq(const void *a, const void *b, int64_t elem_size)
{
    return memcmp(a, b, (size_t)elem_size) == 0;
}

static uint32_t
pgy_set_raw_hash(const void *elem, int64_t elem_size)
{
    return pgy_hash_bytes(elem, (size_t)elem_size);
}

static uint32_t
pgy_set_raw_string_hash_value(const char *value)
{
    return pgy_hash_string_export(value != NULL ? value : "");
}

static bool
pgy_set_raw_string_slot_eq(const void *slot, const char *value)
{
    const char *stored;
    const char *probe;

    stored = slot != NULL ? *(const char *const *)slot : NULL;
    probe = value != NULL ? value : "";
    if (stored == probe)
        return true;
    if (stored == NULL)
        return false;
    return strcmp(stored, probe) == 0;
}

#define SET_RAW_ELEM(set, idx, esz) ((char *)(set)->data + (idx) * (size_t)(esz))

void
pgy_set_new_raw_export(void *set_ptr, int64_t elem_size)
{
    PgySetRaw *set = (PgySetRaw *)set_ptr;
    size_t elem_bytes;
    if (set == NULL) {
        pgy_runtime_warn_invalid_collection("set_new", "null set");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("set_new", "non-positive element size");
        return;
    }
    elem_bytes = (size_t)elem_size;
    set->capacity = 16;
    if (!pgy_set_raw_shape_fits(set->capacity, elem_bytes)) {
        set->capacity = 0;
        pgy_runtime_warn_invalid_collection("set_new", "allocation size overflow");
        return;
    }
    set->count = 0;
    set->data = calloc(set->capacity, elem_bytes);
    set->occupied = (uint8_t *)calloc(set->capacity, sizeof(uint8_t));
    if (set->data == NULL || set->occupied == NULL) {
        free(set->data);
        free(set->occupied);
        set->data = NULL;
        set->occupied = NULL;
        set->capacity = 0;
        pgy_runtime_warn_invalid_collection("set_new", "allocation failed");
    }
}

static void
pgy_set_raw_rehash(PgySetRaw *set, int64_t elem_size)
{
    size_t oc = set->capacity;
    void *od = set->data;
    uint8_t *oo = set->occupied;
    size_t elem_bytes;
    size_t new_capacity;
    void *new_data;
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("set_rehash", "non-positive element size");
        return;
    }
    elem_bytes = (size_t)elem_size;
    if (set->capacity == 0) {
        new_capacity = 16;
    } else {
        if (set->capacity > SIZE_MAX / 2) {
            pgy_runtime_warn_invalid_collection("set_rehash", "capacity overflow");
            return;
        }
        new_capacity = set->capacity * 2;
    }
    if (new_capacity > UINT32_MAX
        || !pgy_set_raw_shape_fits(new_capacity, elem_bytes)
        || new_capacity > SIZE_MAX / sizeof(uint8_t)) {
        pgy_runtime_warn_invalid_collection("set_rehash", "allocation size overflow");
        return;
    }
    new_data = calloc(new_capacity, elem_bytes);
    uint8_t *new_occupied = (uint8_t *)calloc(new_capacity, sizeof(uint8_t));
    if (new_data == NULL || new_occupied == NULL) {
        free(new_data);
        free(new_occupied);
        pgy_runtime_warn_invalid_collection("set_rehash", "allocation failed");
        return;
    }
    set->capacity = new_capacity;
    set->data = new_data;
    set->occupied = new_occupied;
    set->count = 0;
    for (size_t i = 0; i < oc; i++) {
        if (oo[i] == PGY_SET_RAW_LIVE) {
            void *elem = (char *)od + i * elem_bytes;
            uint32_t h = pgy_set_raw_hash(elem, elem_size) % (uint32_t)set->capacity;
            while (set->occupied[h] == PGY_SET_RAW_LIVE)
                h = (h + 1) % (uint32_t)set->capacity;
            memcpy(SET_RAW_ELEM(set, h, elem_size), elem, elem_bytes);
            set->occupied[h] = PGY_SET_RAW_LIVE;
            set->count++;
        }
    }
    free(od);
    free(oo);
}

static void
pgy_set_raw_rehash_string(PgySetRaw *set)
{
    size_t oc = set->capacity;
    void *od = set->data;
    uint8_t *oo = set->occupied;
    size_t new_capacity;
    void *new_data;
    uint8_t *new_occupied;

    if (set->capacity == 0) {
        new_capacity = 16;
    } else {
        if (set->capacity > SIZE_MAX / 2) {
            pgy_runtime_warn_invalid_collection("set_rehash_string",
                "capacity overflow");
            return;
        }
        new_capacity = set->capacity * 2;
    }
    if (new_capacity > UINT32_MAX
        || !pgy_set_raw_shape_fits(new_capacity, sizeof(char *))
        || new_capacity > SIZE_MAX / sizeof(uint8_t)) {
        pgy_runtime_warn_invalid_collection("set_rehash_string",
            "allocation size overflow");
        return;
    }
    new_data = calloc(new_capacity, sizeof(char *));
    new_occupied = (uint8_t *)calloc(new_capacity, sizeof(uint8_t));
    if (new_data == NULL || new_occupied == NULL) {
        free(new_data);
        free(new_occupied);
        pgy_runtime_warn_invalid_collection("set_rehash_string",
            "allocation failed");
        return;
    }
    set->capacity = new_capacity;
    set->data = new_data;
    set->occupied = new_occupied;
    set->count = 0;
    for (size_t i = 0; i < oc; i++) {
        if (oo[i] == PGY_SET_RAW_LIVE) {
            char **slot = (char **)((char *)od + i * sizeof(char *));
            uint32_t h = pgy_set_raw_string_hash_value(*slot)
                % (uint32_t)set->capacity;
            while (set->occupied[h] == PGY_SET_RAW_LIVE)
                h = (h + 1) % (uint32_t)set->capacity;
            memcpy(SET_RAW_ELEM(set, h, sizeof(char *)), slot,
                   sizeof(char *));
            set->occupied[h] = PGY_SET_RAW_LIVE;
            set->count++;
        }
    }
    free(od);
    free(oo);
}

void
pgy_set_add_raw_export(void *set_ptr, void *elem_ptr, int64_t elem_size)
{
    PgySetRaw *set = (PgySetRaw *)set_ptr;
    if (set == NULL) {
        pgy_runtime_warn_invalid_collection("set_add", "null set");
        return;
    }
    if (elem_ptr == NULL) {
        pgy_runtime_warn_invalid_collection("set_add", "null element");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("set_add", "non-positive element size");
        return;
    }
    if (set->capacity == 0 || set->data == NULL || set->occupied == NULL) {
        pgy_runtime_warn_invalid_collection("set_add", "set is not initialized");
        return;
    }
    /* Check if already present */
    uint32_t h = pgy_set_raw_hash(elem_ptr, elem_size) % (uint32_t)set->capacity;
    size_t p = 0;
    uint32_t first_deleted = UINT32_MAX;
    while (set->occupied[h] && p < set->capacity) {
        if (set->occupied[h] == PGY_SET_RAW_LIVE
            && pgy_set_raw_elem_eq(SET_RAW_ELEM(set, h, elem_size), elem_ptr, elem_size))
            return; /* already in set */
        if (set->occupied[h] == PGY_SET_RAW_DELETED && first_deleted == UINT32_MAX)
            first_deleted = h;
        h = (h + 1) % (uint32_t)set->capacity; p++;
    }
    /* Resize if needed */
    if ((double)set->count / (double)set->capacity > 0.75) {
        pgy_set_raw_rehash(set, elem_size);
        if (set->capacity == 0 || set->data == NULL || set->occupied == NULL) {
            pgy_runtime_warn_invalid_collection("set_add", "set rehash failed");
            return;
        }
        h = pgy_set_raw_hash(elem_ptr, elem_size) % (uint32_t)set->capacity;
        while (set->occupied[h] == PGY_SET_RAW_LIVE)
            h = (h + 1) % (uint32_t)set->capacity;
        first_deleted = UINT32_MAX;
    }
    if (first_deleted != UINT32_MAX)
        h = first_deleted;
    memcpy(SET_RAW_ELEM(set, h, elem_size), elem_ptr, (size_t)elem_size);
    set->occupied[h] = PGY_SET_RAW_LIVE;
    set->count++;
}

void
pgy_set_add_string_raw_export(void *set_ptr, const char *value)
{
    PgySetRaw *set = (PgySetRaw *)set_ptr;
    const char *probe = value != NULL ? value : "";
    uint32_t h;
    size_t p = 0;
    uint32_t first_deleted = UINT32_MAX;
    char *owned;

    if (set == NULL) {
        pgy_runtime_warn_invalid_collection("set_add_string", "null set");
        return;
    }
    if (set->capacity == 0 || set->data == NULL || set->occupied == NULL) {
        pgy_runtime_warn_invalid_collection("set_add_string",
            "set is not initialized");
        return;
    }
    h = pgy_set_raw_string_hash_value(probe) % (uint32_t)set->capacity;
    while (set->occupied[h] && p < set->capacity) {
        if (set->occupied[h] == PGY_SET_RAW_LIVE
            && pgy_set_raw_string_slot_eq(
                SET_RAW_ELEM(set, h, sizeof(char *)), probe)) {
            return;
        }
        if (set->occupied[h] == PGY_SET_RAW_DELETED
            && first_deleted == UINT32_MAX) {
            first_deleted = h;
        }
        h = (h + 1) % (uint32_t)set->capacity;
        p++;
    }
    if ((double)set->count / (double)set->capacity > 0.75) {
        pgy_set_raw_rehash_string(set);
        if (set->capacity == 0 || set->data == NULL
            || set->occupied == NULL) {
            pgy_runtime_warn_invalid_collection("set_add_string",
                "set rehash failed");
            return;
        }
        h = pgy_set_raw_string_hash_value(probe) % (uint32_t)set->capacity;
        p = 0;
        first_deleted = UINT32_MAX;
        while (set->occupied[h] && p < set->capacity) {
            if (set->occupied[h] == PGY_SET_RAW_LIVE
                && pgy_set_raw_string_slot_eq(
                    SET_RAW_ELEM(set, h, sizeof(char *)), probe)) {
                return;
            }
            if (set->occupied[h] == PGY_SET_RAW_DELETED
                && first_deleted == UINT32_MAX) {
                first_deleted = h;
            }
            h = (h + 1) % (uint32_t)set->capacity;
            p++;
        }
        if (p >= set->capacity && first_deleted == UINT32_MAX) {
            pgy_runtime_warn_invalid_collection("set_add_string",
                "set has no insertion slot after rehash");
            return;
        }
    }
    if (first_deleted != UINT32_MAX)
        h = first_deleted;
    owned = pgy_runtime_strdup_export(value != NULL ? value : "");
    if (owned == NULL) {
        pgy_runtime_warn_invalid_collection("set_add_string", "string duplication failed");
        return;
    }
    memcpy(SET_RAW_ELEM(set, h, sizeof(char *)), &owned, sizeof(char *));
    set->occupied[h] = PGY_SET_RAW_LIVE;
    set->count++;
}
