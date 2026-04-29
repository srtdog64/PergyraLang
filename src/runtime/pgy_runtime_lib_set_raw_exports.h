#ifndef PGY_RUNTIME_LIB_SET_RAW_EXPORTS_H
#define PGY_RUNTIME_LIB_SET_RAW_EXPORTS_H

bool
pgy_set_has_raw_export(void *set_ptr, void *elem_ptr, int64_t elem_size)
{
    PgySetRaw *set = (PgySetRaw *)set_ptr;
    if (set == NULL) {
        pgy_runtime_warn_invalid_collection("set_has", "null set");
        return false;
    }
    if (elem_ptr == NULL) {
        pgy_runtime_warn_invalid_collection("set_has", "null element");
        return false;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("set_has", "non-positive element size");
        return false;
    }
    if (set->capacity == 0 || set->data == NULL || set->occupied == NULL) {
        pgy_runtime_warn_invalid_collection("set_has", "set is not initialized");
        return false;
    }
    if (set->count == 0)
        return false;
    uint32_t h = pgy_set_raw_hash(elem_ptr, elem_size) % (uint32_t)set->capacity;
    size_t p = 0;
    while (set->occupied[h] && p < set->capacity) {
        if (pgy_set_raw_elem_eq(SET_RAW_ELEM(set, h, elem_size), elem_ptr, elem_size))
            return true;
        h = (h + 1) % (uint32_t)set->capacity; p++;
    }
    return false;
}

void
pgy_set_remove_raw_export(void *set_ptr, void *elem_ptr, int64_t elem_size)
{
    PgySetRaw *set = (PgySetRaw *)set_ptr;
    if (set == NULL) {
        pgy_runtime_warn_invalid_collection("set_remove", "null set");
        return;
    }
    if (elem_ptr == NULL) {
        pgy_runtime_warn_invalid_collection("set_remove", "null element");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("set_remove", "non-positive element size");
        return;
    }
    if (set->capacity == 0 || set->data == NULL || set->occupied == NULL) {
        pgy_runtime_warn_invalid_collection("set_remove", "set is not initialized");
        return;
    }
    if (set->count == 0)
        return;
    uint32_t h = pgy_set_raw_hash(elem_ptr, elem_size) % (uint32_t)set->capacity;
    size_t p = 0;
    while (set->occupied[h] && p < set->capacity) {
        if (pgy_set_raw_elem_eq(SET_RAW_ELEM(set, h, elem_size), elem_ptr, elem_size)) {
            memset(SET_RAW_ELEM(set, h, elem_size), 0, (size_t)elem_size);
            set->occupied[h] = 0;
            set->count--;
            return;
        }
        h = (h + 1) % (uint32_t)set->capacity; p++;
    }
}

int32_t
pgy_set_size_raw_export(void *set_ptr)
{
    PgySetRaw *set = (PgySetRaw *)set_ptr;
    if (set == NULL) {
        pgy_runtime_warn_invalid_collection("set_size", "null set");
        return 0;
    }
    return (int32_t)set->count;
}

#endif /* PGY_RUNTIME_LIB_SET_RAW_EXPORTS_H */
