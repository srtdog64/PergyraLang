/* =================================================================
 * Set raw (type-erased) export functions for LLVM linking
 *
 * Generic hash set using open-addressing with FNV-1a hash on raw bytes.
 * Works for ANY element type (Int, String, Bool, Float, structs)
 * without requiring string conversion.
 *
 * Layout matches PgySet_Generic:
 *   void    *data       - element storage (elem_size * capacity)
 *   uint8_t *occupied   — slot occupancy flags
 *   size_t   count
 *   size_t   capacity
 * ================================================================= */

typedef struct {
    void    *data;
    uint8_t *occupied;
    size_t   count;
    size_t   capacity;
} PgySetRaw;

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
    /* String comparison for pointer-sized string elements */
    if (elem_size == (int64_t)sizeof(char *)) {
        const char *sa = *(const char *const *)a;
        const char *sb = *(const char *const *)b;
        if (sa == sb) return true;
        if (sa == NULL || sb == NULL) return false;
        return strcmp(sa, sb) == 0;
    }
    return memcmp(a, b, (size_t)elem_size) == 0;
}

static uint32_t
pgy_set_raw_hash(const void *elem, int64_t elem_size)
{
    /* String hashing for pointer-sized string elements */
    if (elem_size == (int64_t)sizeof(char *)) {
        const char *s = *(const char *const *)elem;
        return s != NULL ? pgy_hash_string_export(s) : 0;
    }
    return pgy_hash_bytes(elem, (size_t)elem_size);
}

#define SET_RAW_ELEM(set, idx, esz) ((char *)(set)->data + (idx) * (size_t)(esz))

void
pgy_set_new_raw_export(void *set_ptr, int64_t elem_size)
{
    PgySetRaw *set = (PgySetRaw *)set_ptr;
    if (set == NULL) {
        pgy_runtime_warn_invalid_collection("set_new", "null set");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("set_new", "non-positive element size");
        return;
    }
    set->capacity = 16;
    set->count = 0;
    set->data = calloc(set->capacity, (size_t)elem_size);
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
    size_t new_capacity = set->capacity == 0 ? 16 : set->capacity * 2;
    void *new_data = calloc(new_capacity, (size_t)elem_size);
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
        if (oo[i]) {
            void *elem = (char *)od + i * (size_t)elem_size;
            uint32_t h = pgy_set_raw_hash(elem, elem_size) % (uint32_t)set->capacity;
            while (set->occupied[h]) h = (h + 1) % (uint32_t)set->capacity;
            memcpy(SET_RAW_ELEM(set, h, elem_size), elem, (size_t)elem_size);
            set->occupied[h] = 1;
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
    while (set->occupied[h] && p < set->capacity) {
        if (pgy_set_raw_elem_eq(SET_RAW_ELEM(set, h, elem_size), elem_ptr, elem_size))
            return; /* already in set */
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
        while (set->occupied[h]) h = (h + 1) % (uint32_t)set->capacity;
    }
    memcpy(SET_RAW_ELEM(set, h, elem_size), elem_ptr, (size_t)elem_size);
    set->occupied[h] = 1;
    set->count++;
}
