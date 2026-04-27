/* =================================================================
 * ABI Type Layout Lookup
 *
 * Maps Pergyra surface type names (e.g. "Slot<Int>") to explicit
 * MIRTypeLayout structs sourced from pgy_abi_spec.h.
 *
 * Backends MUST use these lookups instead of inventing their own
 * struct layouts.
 * ================================================================= */

/* Static ABI type table ??populated at MIR lower time */
static MIRTypeLayout g_abi_type_table[64];
static size_t g_abi_type_count = 0;

static MIRTypeLayout *
abi_type_lookup_by_name(const char *pergyra_type_name)
{
    if (pergyra_type_name == NULL)
        return NULL;

    for (size_t i = 0; i < g_abi_type_count; i++) {
        if (g_abi_type_table[i].abi_type_name != NULL
            && strcmp(g_abi_type_table[i].abi_type_name, pergyra_type_name) == 0) {
            return &g_abi_type_table[i];
        }
    }
    return NULL;
}

static MIRTypeLayout *
abi_type_lookup_by_runtime_fn(const char *runtime_fn)
{
    if (runtime_fn == NULL)
        return NULL;

    for (size_t i = 0; i < g_abi_type_count; i++) {
        if (g_abi_type_table[i].runtime_fn != NULL
            && strcmp(g_abi_type_table[i].runtime_fn, runtime_fn) == 0) {
            return &g_abi_type_table[i];
        }
    }
    return NULL;
}

static void
abi_type_table_init(void)
{
    g_abi_type_count = 0;

    /* Helper macro to add a type layout */
#define ADD_TYPE(out, name, abi, size, align, fn, inner) \
    do { \
        (out) = &g_abi_type_table[g_abi_type_count++]; \
        memset((out), 0, sizeof(*(out))); \
        (out)->abi_type_name = (name); \
        (out)->size_bytes = (size); \
        (out)->align_bytes = (align); \
        (out)->runtime_fn = (fn); \
        (out)->inner_c_type = (inner); \
    } while (0)

#define ADD_FIELD_STRUCT(out, field_label, struct_type, member) \
    do { \
        if ((out)->field_count < MIR_MAX_TYPE_FIELDS) { \
            MIRFieldLayout *f = &(out)->fields[(out)->field_count++]; \
            f->field_name = (field_label); \
            f->offset = offsetof(struct_type, member); \
            f->field_size = sizeof(((struct_type *)0)->member); \
            f->field_align = _Alignof(((struct_type *)0)->member); \
        } \
    } while (0)

#define ADD_FIELD_SCALAR(out, field_label, scalar_type) \
    do { \
        if ((out)->field_count < MIR_MAX_TYPE_FIELDS) { \
            MIRFieldLayout *f = &(out)->fields[(out)->field_count++]; \
            f->field_name = (field_label); \
            f->offset = 0; \
            f->field_size = sizeof(scalar_type); \
            f->field_align = _Alignof(scalar_type); \
        } \
    } while (0)

    MIRTypeLayout *t = NULL;

    /* Slot<T> ??Debug mode */
    ADD_TYPE(t, "Slot<Int>",    "pgy_abi_slot_int_dbg",     8,  4, "pgy_claim_Int",     "int32_t");
    ADD_FIELD_STRUCT(t, "value", pgy_abi_slot_int_dbg, value);
    ADD_FIELD_STRUCT(t, "occupied", pgy_abi_slot_int_dbg, occupied);
    ADD_TYPE(t, "Slot<Long>",   "pgy_abi_slot_long_dbg",   16,  8, "pgy_claim_Long",    "int64_t");
    ADD_FIELD_STRUCT(t, "value", pgy_abi_slot_long_dbg, value);
    ADD_FIELD_STRUCT(t, "occupied", pgy_abi_slot_long_dbg, occupied);
    ADD_TYPE(t, "Slot<Float>",  "pgy_abi_slot_float_dbg",   8,  4, "pgy_claim_Float",   "float");
    ADD_FIELD_STRUCT(t, "value", pgy_abi_slot_float_dbg, value);
    ADD_FIELD_STRUCT(t, "occupied", pgy_abi_slot_float_dbg, occupied);
    ADD_TYPE(t, "Slot<Double>", "pgy_abi_slot_double_dbg", 16,  8, "pgy_claim_Double",  "double");
    ADD_FIELD_STRUCT(t, "value", pgy_abi_slot_double_dbg, value);
    ADD_FIELD_STRUCT(t, "occupied", pgy_abi_slot_double_dbg, occupied);
    ADD_TYPE(t, "Slot<Bool>",   "pgy_abi_slot_bool_dbg",    2,  1, "pgy_claim_Bool",    "bool");
    ADD_FIELD_STRUCT(t, "value", pgy_abi_slot_bool_dbg, value);
    ADD_FIELD_STRUCT(t, "occupied", pgy_abi_slot_bool_dbg, occupied);
    ADD_TYPE(t, "Slot<String>", "pgy_abi_slot_string_dbg", 16,  8, "pgy_claim_String",  "char*");
    ADD_FIELD_STRUCT(t, "value", pgy_abi_slot_string_dbg, value);
    ADD_FIELD_STRUCT(t, "occupied", pgy_abi_slot_string_dbg, occupied);

    /* Slot<T> ??Release mode (sizes differ, but fn names are same) */
    ADD_TYPE(t, "Slot<Int>_rel",    "pgy_abi_slot_int_rel",    4, 4, "pgy_claim_Int",    "int32_t");
    ADD_FIELD_STRUCT(t, "value", pgy_abi_slot_int_rel, value);
    ADD_TYPE(t, "Slot<Long>_rel",   "pgy_abi_slot_long_rel",   8, 8, "pgy_claim_Long",   "int64_t");
    ADD_FIELD_STRUCT(t, "value", pgy_abi_slot_long_rel, value);
    ADD_TYPE(t, "Slot<Float>_rel",  "pgy_abi_slot_float_rel",  4, 4, "pgy_claim_Float",  "float");
    ADD_FIELD_STRUCT(t, "value", pgy_abi_slot_float_rel, value);
    ADD_TYPE(t, "Slot<Double>_rel", "pgy_abi_slot_double_rel", 8, 8, "pgy_claim_Double", "double");
    ADD_FIELD_STRUCT(t, "value", pgy_abi_slot_double_rel, value);
    ADD_TYPE(t, "Slot<Bool>_rel",   "pgy_abi_slot_bool_rel",   1, 1, "pgy_claim_Bool",   "bool");
    ADD_FIELD_STRUCT(t, "value", pgy_abi_slot_bool_rel, value);
    ADD_TYPE(t, "Slot<String>_rel", "pgy_abi_slot_string_rel", 8, 8, "pgy_claim_String", "char*");
    ADD_FIELD_STRUCT(t, "value", pgy_abi_slot_string_rel, value);

    /* SecureSlot<T> */
    ADD_TYPE(t, "SecureSlot<Int>",    "pgy_abi_secure_slot_int_dbg", 16, 8, "pgy_claim_secure_Int",    "int32_t");
    ADD_FIELD_STRUCT(t, "value", pgy_abi_secure_slot_int_dbg, value);
    ADD_FIELD_STRUCT(t, "occupied", pgy_abi_secure_slot_int_dbg, occupied);
    ADD_FIELD_STRUCT(t, "token", pgy_abi_secure_slot_int_dbg, token);
    ADD_TYPE(t, "SecureSlot<String>", "pgy_abi_secure_slot_string_dbg", 24, 8, "pgy_claim_secure_String", "char*");
    ADD_FIELD_STRUCT(t, "value", pgy_abi_secure_slot_string_dbg, value);
    ADD_FIELD_STRUCT(t, "occupied", pgy_abi_secure_slot_string_dbg, occupied);
    ADD_FIELD_STRUCT(t, "token", pgy_abi_secure_slot_string_dbg, token);

    /* Pin/lease views */
    ADD_TYPE(t, "PinnedSlotView<Int>", "pgy_abi_pinned_slot_view_int",
             sizeof(pgy_abi_pinned_slot_view_int), _Alignof(pgy_abi_pinned_slot_view_int),
             "pgy_pin_read_Int", "int32_t");
    ADD_FIELD_STRUCT(t, "slot", pgy_abi_pinned_slot_view_int, slot);
    ADD_FIELD_STRUCT(t, "active", pgy_abi_pinned_slot_view_int, active);
    ADD_FIELD_STRUCT(t, "can_write", pgy_abi_pinned_slot_view_int, can_write);
    ADD_TYPE(t, "PinnedSecureSlotView<Int>", "pgy_abi_pinned_secure_slot_view_int",
             sizeof(pgy_abi_pinned_secure_slot_view_int), _Alignof(pgy_abi_pinned_secure_slot_view_int),
             "pgy_secure_pin_read_Int", "int32_t");
    ADD_FIELD_STRUCT(t, "slot", pgy_abi_pinned_secure_slot_view_int, slot);
    ADD_FIELD_STRUCT(t, "token", pgy_abi_pinned_secure_slot_view_int, token);
    ADD_FIELD_STRUCT(t, "active", pgy_abi_pinned_secure_slot_view_int, active);
    ADD_FIELD_STRUCT(t, "can_write", pgy_abi_pinned_secure_slot_view_int, can_write);

    /* DeviceSlot<T> */
    ADD_TYPE(t, "DeviceSlot<Int>",    "pgy_abi_device_slot_int",    8,  4, "pgy_claim_device_Int",    "int32_t");
    ADD_FIELD_STRUCT(t, "value", pgy_abi_device_slot_int, value);
    ADD_FIELD_STRUCT(t, "claimed", pgy_abi_device_slot_int, claimed);
    ADD_TYPE(t, "DeviceSlot<String>", "pgy_abi_device_slot_string", 16, 8, "pgy_claim_device_String", "char*");
    ADD_FIELD_STRUCT(t, "value", pgy_abi_device_slot_string, value);
    ADD_FIELD_STRUCT(t, "claimed", pgy_abi_device_slot_string, claimed);

    /* Option<T> */
    ADD_TYPE(t, "Option<Int>",    "pgy_abi_option_int",     8,  4, "pgy_option_some_Int",    "int32_t");
    ADD_FIELD_STRUCT(t, "tag", pgy_abi_option_int, tag);
    ADD_FIELD_STRUCT(t, "value", pgy_abi_option_int, value);
    ADD_TYPE(t, "Option<Long>",   "pgy_abi_option_long",   16,  8, "pgy_option_some_Long",   "int64_t");
    ADD_FIELD_STRUCT(t, "tag", pgy_abi_option_long, tag);
    ADD_FIELD_STRUCT(t, "value", pgy_abi_option_long, value);
    ADD_TYPE(t, "Option<Bool>",   "pgy_abi_option_bool",    8,  4, "pgy_option_some_Bool",   "bool");
    ADD_FIELD_STRUCT(t, "tag", pgy_abi_option_bool, tag);
    ADD_FIELD_STRUCT(t, "value", pgy_abi_option_bool, value);
    ADD_TYPE(t, "Option<String>", "pgy_abi_option_string", 16,  8, "pgy_option_some_String", "char*");
    ADD_FIELD_STRUCT(t, "tag", pgy_abi_option_string, tag);
    ADD_FIELD_STRUCT(t, "value", pgy_abi_option_string, value);

    /* Result<T, E> */
    ADD_TYPE(t, "Result<Int>",    "pgy_abi_result_int",    16, 8, "pgy_result_ok_Int",    "int32_t");
    ADD_FIELD_STRUCT(t, "tag", pgy_abi_result_int, tag);
    ADD_FIELD_STRUCT(t, "ok", pgy_abi_result_int, ok);
    ADD_FIELD_STRUCT(t, "err", pgy_abi_result_int, err);
    ADD_TYPE(t, "Result<Bool>",   "pgy_abi_result_bool",   16, 8, "pgy_result_ok_Bool",   "bool");
    ADD_FIELD_STRUCT(t, "tag", pgy_abi_result_bool, tag);
    ADD_FIELD_STRUCT(t, "ok", pgy_abi_result_bool, ok);
    ADD_FIELD_STRUCT(t, "err", pgy_abi_result_bool, err);
    ADD_TYPE(t, "Result<String>", "pgy_abi_result_string", 16, 8, "pgy_result_ok_String", "char*");
    ADD_FIELD_STRUCT(t, "tag", pgy_abi_result_string, tag);
    ADD_FIELD_STRUCT(t, "ok", pgy_abi_result_string, ok);
    ADD_FIELD_STRUCT(t, "err", pgy_abi_result_string, err);

    /* Channel ??opaque handles */
    ADD_TYPE(t, "ZoneChannel<Int>",    "pgy_abi_zone_channel_handle",    4, 4, "pgy_zone_channel_create_Int",    "int32_t");
    ADD_FIELD_SCALAR(t, "handle", pgy_abi_zone_channel_handle);
    ADD_TYPE(t, "WorldChannel<Int>",   "pgy_abi_world_channel_handle",   4, 4, "pgy_world_channel_create_Int",   "int32_t");
    ADD_FIELD_SCALAR(t, "handle", pgy_abi_world_channel_handle);
    ADD_TYPE(t, "ZoneChannel<String>", "pgy_abi_zone_channel_handle",    4, 4, "pgy_zone_channel_create_String", "char*");
    ADD_FIELD_SCALAR(t, "handle", pgy_abi_zone_channel_handle);

    /* Box<T> */
    ADD_TYPE(t, "Box<Int>",    "pgy_abi_box_int",    8, 8, "pgy_box_new_Int",    "int32_t");
    ADD_FIELD_STRUCT(t, "ptr", pgy_abi_box_int, ptr);
    ADD_TYPE(t, "Box<String>", "pgy_abi_box_string", 8, 8, "pgy_box_new_String", "char*");
    ADD_FIELD_STRUCT(t, "ptr", pgy_abi_box_string, ptr);

    /* Array<T> */
    ADD_TYPE(t, "Array<Int>",    "pgy_abi_array_int", 24, 8, "pgy_array_new_Int",    "int32_t");
    ADD_FIELD_STRUCT(t, "data", pgy_abi_array_int, data);
    ADD_FIELD_STRUCT(t, "len", pgy_abi_array_int, len);
    ADD_FIELD_STRUCT(t, "cap", pgy_abi_array_int, cap);
    ADD_TYPE(t, "Array<String>", "pgy_abi_array_string", 24, 8, "pgy_array_new_String", "char*");
    ADD_FIELD_STRUCT(t, "data", pgy_abi_array_string, data);
    ADD_FIELD_STRUCT(t, "len", pgy_abi_array_string, len);
    ADD_FIELD_STRUCT(t, "cap", pgy_abi_array_string, cap);

    /* Auxiliary */
    ADD_TYPE(t, "Future",           "pgy_abi_future",          8, 4, "pgy_spawn",            "int32_t");
    ADD_FIELD_STRUCT(t, "handle", pgy_abi_future, handle);
    ADD_FIELD_STRUCT(t, "ready", pgy_abi_future, ready);
    ADD_TYPE(t, "RemoteFuture",     "pgy_abi_remote_future",  24, 8, "pgy_spawn",            "int32_t");
    ADD_FIELD_STRUCT(t, "handle", pgy_abi_remote_future, handle);
    ADD_FIELD_STRUCT(t, "ready", pgy_abi_remote_future, ready);
    ADD_FIELD_STRUCT(t, "trace_id", pgy_abi_remote_future, trace_id);
    ADD_FIELD_STRUCT(t, "trace_data", pgy_abi_remote_future, trace_data);
    ADD_TYPE(t, "Qubit",            "pgy_abi_qubit",          12, 4, "ClaimQubit",           "int32_t");
    ADD_FIELD_STRUCT(t, "state", pgy_abi_qubit, state);
    ADD_FIELD_STRUCT(t, "pool_id", pgy_abi_qubit, pool_id);
    ADD_FIELD_STRUCT(t, "measured", pgy_abi_qubit, measured);
    ADD_TYPE(t, "TaskHandle",       "pgy_abi_task_handle",     8, 4, NULL,                   "int32_t");
    ADD_FIELD_STRUCT(t, "id", pgy_abi_task_handle, id);
    ADD_FIELD_STRUCT(t, "valid", pgy_abi_task_handle, valid);
    ADD_TYPE(t, "Timer",            "pgy_abi_timer",          12, 4, NULL,                   "int32_t");
    ADD_FIELD_STRUCT(t, "duration", pgy_abi_timer, duration);
    ADD_FIELD_STRUCT(t, "remaining", pgy_abi_timer, remaining);
    ADD_FIELD_STRUCT(t, "done", pgy_abi_timer, done);
    ADD_TYPE(t, "Arena",            "pgy_abi_arena",          24, 8, NULL,                   "char*");
    ADD_FIELD_STRUCT(t, "buffer", pgy_abi_arena, buffer);
    ADD_FIELD_STRUCT(t, "capacity", pgy_abi_arena, capacity);
    ADD_FIELD_STRUCT(t, "offset", pgy_abi_arena, offset);
    ADD_TYPE(t, "Allocator",        "pgy_abi_allocator",      48, 8, NULL,                   "void*");
    ADD_FIELD_STRUCT(t, "kind", pgy_abi_allocator, kind);
    ADD_FIELD_STRUCT(t, "trace_enabled", pgy_abi_allocator, trace_enabled);
    ADD_FIELD_STRUCT(t, "debug_enabled", pgy_abi_allocator, debug_enabled);
    ADD_FIELD_STRUCT(t, "allocations", pgy_abi_allocator, allocations);
    ADD_FIELD_STRUCT(t, "deallocations", pgy_abi_allocator, deallocations);
    ADD_FIELD_STRUCT(t, "bytes_in_use", pgy_abi_allocator, bytes_in_use);
    ADD_FIELD_STRUCT(t, "peak_bytes", pgy_abi_allocator, peak_bytes);
    ADD_FIELD_STRUCT(t, "pool", pgy_abi_allocator, pool);

#undef ADD_TYPE
#undef ADD_FIELD_STRUCT
#undef ADD_FIELD_SCALAR
}

/* Extract inner type from "Slot<Int>" ??"Int" */
static const char *
mir_extract_inner_type_suffix(const char *pergyra_type_name)
{
    if (pergyra_type_name == NULL)
        return NULL;
    const char *lt = strchr(pergyra_type_name, '<');
    const char *gt = strrchr(pergyra_type_name, '>');
    if (lt == NULL || gt == NULL || gt <= lt)
        return NULL;
    /* Return substring between < and > */
    static char buf[64];
    size_t len = (size_t)(gt - lt - 1);
    if (len >= sizeof(buf))
        len = sizeof(buf) - 1;
    memcpy(buf, lt + 1, len);
    buf[len] = '\0';
    return buf;
}

/* Lookup ABI type by Pergyra type name.
 *
 * Mode selection:
 *   1. Exact match for the given name (covers debug-mode names like "Slot<Int>"
 *      and non-generic types like "Future", "Qubit", etc.)
 *   2. If not found and the name looks like a Slot/Option/Result generic type,
 *      try the _rel suffix variant (release mode).
 *   3. Fall back to runtime function name pattern matching for any generic type.
 *   4. Fall back to exact match for non-generic auxiliary types.
 */
const MIRTypeLayout *
mir_abi_lookup(const char *pergyra_type_name)
{
    int written;

    if (pergyra_type_name == NULL)
        return NULL;

    /* Step 1: Exact match (debug mode names and non-generic types) */
    MIRTypeLayout *t = abi_type_lookup_by_name(pergyra_type_name);
    if (t != NULL)
        return t;

    /* Step 2: Try release mode variant for Slot/Option/Result generics */
    char rel_name[128];
    written = snprintf(rel_name, sizeof(rel_name), "%s_rel", pergyra_type_name);
    if (written >= 0 && (size_t)written < sizeof(rel_name)) {
        t = abi_type_lookup_by_name(rel_name);
        if (t != NULL)
            return t;
    }

    /* Try to find by runtime function name pattern */
    /* e.g. "Slot<Int>" -> look for any type with "pgy_claim_Int" */
    const char *suffix = mir_extract_inner_type_suffix(pergyra_type_name);
    if (suffix != NULL) {
        char fn_prefix[128];

        /* Check if it's a Slot type */
        if (strncmp(pergyra_type_name, "Slot<", 5) == 0) {
            written = snprintf(fn_prefix, sizeof(fn_prefix), "pgy_claim_%s", suffix);
            if (written >= 0 && (size_t)written < sizeof(fn_prefix)) {
                t = abi_type_lookup_by_runtime_fn(fn_prefix);
                if (t != NULL)
                    return t;
            }
        }
        /* Option */
        else if (strncmp(pergyra_type_name, "Option<", 7) == 0) {

            written = snprintf(fn_prefix, sizeof(fn_prefix), "pgy_option_some_%s", suffix);
            if (written >= 0 && (size_t)written < sizeof(fn_prefix)) {
                t = abi_type_lookup_by_runtime_fn(fn_prefix);
                if (t != NULL)
                    return t;
            }
        }
        /* Result */
        else if (strncmp(pergyra_type_name, "Result<", 7) == 0) {
            written = snprintf(fn_prefix, sizeof(fn_prefix), "pgy_result_ok_%s", suffix);
            if (written >= 0 && (size_t)written < sizeof(fn_prefix)) {
                t = abi_type_lookup_by_runtime_fn(fn_prefix);
                if (t != NULL)
                    return t;
            }
        }
        /* ZoneChannel<T> */
        else if (strncmp(pergyra_type_name, "ZoneChannel<", 12) == 0) {
            written = snprintf(fn_prefix, sizeof(fn_prefix), "pgy_zone_channel_create_%s", suffix);
            if (written >= 0 && (size_t)written < sizeof(fn_prefix)) {
                t = abi_type_lookup_by_runtime_fn(fn_prefix);
                if (t != NULL)
                    return t;
            }
        }
        /* WorldChannel<T> */
        else if (strncmp(pergyra_type_name, "WorldChannel<", 13) == 0) {
            written = snprintf(fn_prefix, sizeof(fn_prefix), "pgy_world_channel_create_%s", suffix);
            if (written >= 0 && (size_t)written < sizeof(fn_prefix)) {
                t = abi_type_lookup_by_runtime_fn(fn_prefix);
                if (t != NULL)
                    return t;
            }
        }
        /* Box<T> */
        else if (strncmp(pergyra_type_name, "Box<", 4) == 0) {
            written = snprintf(fn_prefix, sizeof(fn_prefix), "pgy_box_new_%s", suffix);
            if (written >= 0 && (size_t)written < sizeof(fn_prefix)) {
                t = abi_type_lookup_by_runtime_fn(fn_prefix);
                if (t != NULL)
                    return t;
            }
        }
        /* Array<T> */
        else if (strncmp(pergyra_type_name, "Array<", 6) == 0) {
            written = snprintf(fn_prefix, sizeof(fn_prefix), "pgy_array_new_%s", suffix);
            if (written >= 0 && (size_t)written < sizeof(fn_prefix)) {
                t = abi_type_lookup_by_runtime_fn(fn_prefix);
                if (t != NULL)
                    return t;
            }
        }
    }

    /* Fall back to exact match for non-generic types (Future, Qubit, TaskHandle, etc.) */
    t = abi_type_lookup_by_name(pergyra_type_name);
    if (t != NULL)
        return t;

    return NULL;  /* unknown type - backend should handle gracefully */
}

void
mir_abi_table_init(void)
{
    abi_type_table_init();
}
