/* Runtime exports for Set<T> -> Array<T> stable snapshots.
 * This owner sits after raw set and array exports in pgy_runtime_lib.c's
 * include graph: it consumes PgySetRaw and returns result-owned PgyArray_*.
 */

void
pgy_set_values_raw_i32_export(void *set_ptr, void *out_array_ptr)
{
    PgySetRaw *set = (PgySetRaw *)set_ptr;
    PgyArray_Int *out = (PgyArray_Int *)out_array_ptr;

    if (out == NULL) {
        pgy_runtime_warn_invalid_collection("set_values_i32", "null output");
        return;
    }
    *out = pgy_array_new_Int(set != NULL ? set->count : 0);
    if (set == NULL) {
        pgy_runtime_warn_invalid_collection("set_values_i32", "null set");
        return;
    }
    if (!pgy_set_raw_is_initialized(set) || set->count == 0)
        return;
    for (size_t i = 0; i < set->capacity; i++) {
        if (set->occupied[i] != PGY_SET_RAW_LIVE)
            continue;
        pgy_array_push_Int(out,
            *(int32_t *)SET_RAW_ELEM(set, i, sizeof(int32_t)));
    }
    pgy_array_sort_Int(out->data, out->length);
}

void
pgy_set_values_raw_i64_export(void *set_ptr, void *out_array_ptr)
{
    PgySetRaw *set = (PgySetRaw *)set_ptr;
    PgyArray_Long *out = (PgyArray_Long *)out_array_ptr;

    if (out == NULL) {
        pgy_runtime_warn_invalid_collection("set_values_i64", "null output");
        return;
    }
    *out = pgy_array_new_Long(set != NULL ? set->count : 0);
    if (set == NULL) {
        pgy_runtime_warn_invalid_collection("set_values_i64", "null set");
        return;
    }
    if (!pgy_set_raw_is_initialized(set) || set->count == 0)
        return;
    for (size_t i = 0; i < set->capacity; i++) {
        if (set->occupied[i] != PGY_SET_RAW_LIVE)
            continue;
        pgy_array_push_Long(out,
            *(int64_t *)SET_RAW_ELEM(set, i, sizeof(int64_t)));
    }
    pgy_array_sort_Long(out->data, out->length);
}

void
pgy_set_values_raw_bool_export(void *set_ptr, void *out_array_ptr)
{
    PgySetRaw *set = (PgySetRaw *)set_ptr;
    PgyArray_Bool *out = (PgyArray_Bool *)out_array_ptr;

    if (out == NULL) {
        pgy_runtime_warn_invalid_collection("set_values_bool", "null output");
        return;
    }
    *out = pgy_array_new_Bool(set != NULL ? set->count : 0);
    if (set == NULL) {
        pgy_runtime_warn_invalid_collection("set_values_bool", "null set");
        return;
    }
    if (!pgy_set_raw_is_initialized(set) || set->count == 0)
        return;
    for (size_t i = 0; i < set->capacity; i++) {
        if (set->occupied[i] != PGY_SET_RAW_LIVE)
            continue;
        pgy_array_push_Bool(out,
            *(bool *)SET_RAW_ELEM(set, i, sizeof(bool)));
    }
    pgy_array_sort_Bool(out->data, out->length);
}

void
pgy_set_values_raw_string_export(void *set_ptr, void *out_array_ptr)
{
    PgySetRaw *set = (PgySetRaw *)set_ptr;
    PgyArray_String *out = (PgyArray_String *)out_array_ptr;

    if (out == NULL) {
        pgy_runtime_warn_invalid_collection("set_values_string", "null output");
        return;
    }
    *out = pgy_array_new_String(set != NULL ? set->count : 0);
    if (set == NULL) {
        pgy_runtime_warn_invalid_collection("set_values_string", "null set");
        return;
    }
    if (!pgy_set_raw_is_initialized(set) || set->count == 0)
        return;
    for (size_t i = 0; i < set->capacity; i++) {
        char *value;

        if (set->occupied[i] != PGY_SET_RAW_LIVE)
            continue;
        value = *(char **)SET_RAW_ELEM(set, i, sizeof(char *));
        pgy_array_push_String(out, value != NULL ? value : "");
    }
    pgy_array_sort_String(out->data, out->length);
}
