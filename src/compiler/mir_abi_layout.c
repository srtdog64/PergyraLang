#include "mir_abi_layout.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../runtime/pgy_abi_spec.h"
/* =================================================================
 * ABI Type Layout Lookup
 *
 * Maps Pergyra surface type names (e.g. "Slot<Int>") to explicit
 * MIRTypeLayout structs sourced from pgy_abi_spec.h. The layout key stored
 * in MIRTypeLayout is the canonical surface type, not the C ABI typedef name.
 *
 * Backends MUST use these lookups instead of inventing their own
 * struct layouts.
 * ================================================================= */

#define ABI_FIELD_STRUCT(field_label, struct_type, member) \
    { (field_label), offsetof(struct_type, member), \
      sizeof(((struct_type *)0)->member), _Alignof(((struct_type *)0)->member) }

#define ABI_FIELD_SCALAR(field_label, scalar_type) \
    { (field_label), 0, sizeof(scalar_type), _Alignof(scalar_type) }

#define ABI_TYPE(name, size, align, fn, inner, count, ...) \
    { (name), (size), (align), (count), { __VA_ARGS__ }, (fn), (inner), \
      MIR_ABI_REPR_UNTAGGED, NULL, 0, 0, NULL }

#define ABI_TAGGED_TYPE(name, size, align, fn, inner, count, tag_field, primary_tag, secondary_tag, ...) \
    { (name), (size), (align), (count), { __VA_ARGS__ }, (fn), (inner), \
      MIR_ABI_REPR_EXPLICIT_TAG, (tag_field), (primary_tag), (secondary_tag), NULL }

static const MIRTypeLayout k_abi_type_table[] = {
    /* Slot<T>: canonical checked ABI. */
    ABI_TYPE("Slot<Int>", 8, 4, "pgy_claim_Int", "int32_t", 2,
             ABI_FIELD_STRUCT("value", pgy_abi_slot_int, value),
             ABI_FIELD_STRUCT("occupied", pgy_abi_slot_int, occupied)),
    ABI_TYPE("Slot<Long>", 16, 8, "pgy_claim_Long", "int64_t", 2,
             ABI_FIELD_STRUCT("value", pgy_abi_slot_long, value),
             ABI_FIELD_STRUCT("occupied", pgy_abi_slot_long, occupied)),
    ABI_TYPE("Slot<Float>", 8, 4, "pgy_claim_Float", "float", 2,
             ABI_FIELD_STRUCT("value", pgy_abi_slot_float, value),
             ABI_FIELD_STRUCT("occupied", pgy_abi_slot_float, occupied)),
    ABI_TYPE("Slot<Double>", 16, 8, "pgy_claim_Double", "double", 2,
             ABI_FIELD_STRUCT("value", pgy_abi_slot_double, value),
             ABI_FIELD_STRUCT("occupied", pgy_abi_slot_double, occupied)),
    ABI_TYPE("Slot<Bool>", 2, 1, "pgy_claim_Bool", "bool", 2,
             ABI_FIELD_STRUCT("value", pgy_abi_slot_bool, value),
             ABI_FIELD_STRUCT("occupied", pgy_abi_slot_bool, occupied)),
    ABI_TYPE("Slot<String>", 16, 8, "pgy_claim_String", "char*", 2,
             ABI_FIELD_STRUCT("value", pgy_abi_slot_string, value),
             ABI_FIELD_STRUCT("occupied", pgy_abi_slot_string, occupied)),

    /* SecureSlot<T> */
    ABI_TYPE("SecureSlot<Int>", 16, 8, "pgy_claim_secure_Int", "int32_t", 3,
             ABI_FIELD_STRUCT("value", pgy_abi_secure_slot_int, value),
             ABI_FIELD_STRUCT("occupied", pgy_abi_secure_slot_int, occupied),
             ABI_FIELD_STRUCT("token", pgy_abi_secure_slot_int, token)),
    ABI_TYPE("SecureSlot<String>", 24, 8, "pgy_claim_secure_String", "char*", 3,
             ABI_FIELD_STRUCT("value", pgy_abi_secure_slot_string, value),
             ABI_FIELD_STRUCT("occupied", pgy_abi_secure_slot_string, occupied),
             ABI_FIELD_STRUCT("token", pgy_abi_secure_slot_string, token)),

    /* Pin/lease views */
    ABI_TYPE("PinnedSlotView<Int>",
             sizeof(pgy_abi_pinned_slot_view_int),
             _Alignof(pgy_abi_pinned_slot_view_int),
             "pgy_pin_read_Int", "int32_t", 3,
             ABI_FIELD_STRUCT("slot", pgy_abi_pinned_slot_view_int, slot),
             ABI_FIELD_STRUCT("active", pgy_abi_pinned_slot_view_int, active),
             ABI_FIELD_STRUCT("can_write", pgy_abi_pinned_slot_view_int, can_write)),
    ABI_TYPE("PinnedSecureSlotView<Int>",
             sizeof(pgy_abi_pinned_secure_slot_view_int),
             _Alignof(pgy_abi_pinned_secure_slot_view_int),
             "pgy_secure_pin_read_Int", "int32_t", 4,
             ABI_FIELD_STRUCT("slot", pgy_abi_pinned_secure_slot_view_int, slot),
             ABI_FIELD_STRUCT("token", pgy_abi_pinned_secure_slot_view_int, token),
             ABI_FIELD_STRUCT("active", pgy_abi_pinned_secure_slot_view_int, active),
             ABI_FIELD_STRUCT("can_write", pgy_abi_pinned_secure_slot_view_int, can_write)),

    /* DeviceSlot<T> */
    ABI_TYPE("DeviceSlot<Int>", 8, 4, "pgy_claim_device_Int", "int32_t", 2,
             ABI_FIELD_STRUCT("value", pgy_abi_device_slot_int, value),
             ABI_FIELD_STRUCT("claimed", pgy_abi_device_slot_int, claimed)),
    ABI_TYPE("DeviceSlot<String>", 16, 8, "pgy_claim_device_String", "char*", 2,
             ABI_FIELD_STRUCT("value", pgy_abi_device_slot_string, value),
             ABI_FIELD_STRUCT("claimed", pgy_abi_device_slot_string, claimed)),

    /* Option<T> */
    ABI_TAGGED_TYPE("Option<Int>", 8, 4, "pgy_option_some_Int", "int32_t", 2,
             "tag", PgyAbiOptionSome, PgyAbiOptionNone,
             ABI_FIELD_STRUCT("tag", pgy_abi_option_int, tag),
             ABI_FIELD_STRUCT("value", pgy_abi_option_int, value)),
    ABI_TAGGED_TYPE("Option<Long>", 16, 8, "pgy_option_some_Long", "int64_t", 2,
             "tag", PgyAbiOptionSome, PgyAbiOptionNone,
             ABI_FIELD_STRUCT("tag", pgy_abi_option_long, tag),
             ABI_FIELD_STRUCT("value", pgy_abi_option_long, value)),
    ABI_TAGGED_TYPE("Option<Float>", 8, 4, "pgy_option_some_Float", "float", 2,
             "tag", PgyAbiOptionSome, PgyAbiOptionNone,
             ABI_FIELD_STRUCT("tag", pgy_abi_option_float, tag),
             ABI_FIELD_STRUCT("value", pgy_abi_option_float, value)),
    ABI_TAGGED_TYPE("Option<Double>", 16, 8, "pgy_option_some_Double", "double", 2,
             "tag", PgyAbiOptionSome, PgyAbiOptionNone,
             ABI_FIELD_STRUCT("tag", pgy_abi_option_double, tag),
             ABI_FIELD_STRUCT("value", pgy_abi_option_double, value)),
    ABI_TAGGED_TYPE("Option<Bool>", 8, 4, "pgy_option_some_Bool", "bool", 2,
             "tag", PgyAbiOptionSome, PgyAbiOptionNone,
             ABI_FIELD_STRUCT("tag", pgy_abi_option_bool, tag),
             ABI_FIELD_STRUCT("value", pgy_abi_option_bool, value)),
    ABI_TAGGED_TYPE("Option<String>", 16, 8, "pgy_option_some_String", "char*", 2,
             "tag", PgyAbiOptionSome, PgyAbiOptionNone,
             ABI_FIELD_STRUCT("tag", pgy_abi_option_string, tag),
             ABI_FIELD_STRUCT("value", pgy_abi_option_string, value)),

    /* Result<T, E> */
    ABI_TAGGED_TYPE("Result<Int>", 16, 8, "pgy_result_ok_Int", "int32_t", 3,
             "tag", PgyAbiResultOk, PgyAbiResultErr,
             ABI_FIELD_STRUCT("tag", pgy_abi_result_int, tag),
             ABI_FIELD_STRUCT("ok", pgy_abi_result_int, ok),
             ABI_FIELD_STRUCT("err", pgy_abi_result_int, err)),
    ABI_TAGGED_TYPE("Result<Bool>", 16, 8, "pgy_result_ok_Bool", "bool", 3,
             "tag", PgyAbiResultOk, PgyAbiResultErr,
             ABI_FIELD_STRUCT("tag", pgy_abi_result_bool, tag),
             ABI_FIELD_STRUCT("ok", pgy_abi_result_bool, ok),
             ABI_FIELD_STRUCT("err", pgy_abi_result_bool, err)),
    ABI_TAGGED_TYPE("Result<String>", 16, 8, "pgy_result_ok_String", "char*", 3,
             "tag", PgyAbiResultOk, PgyAbiResultErr,
             ABI_FIELD_STRUCT("tag", pgy_abi_result_string, tag),
             ABI_FIELD_STRUCT("ok", pgy_abi_result_string, ok),
             ABI_FIELD_STRUCT("err", pgy_abi_result_string, err)),

    /* Zone/World channel opaque handles. Ordinary Channel<T> is still
     * beta-local runtime storage until movable handle lowering lands. */
    ABI_TYPE("ZoneChannel<Int>", 4, 4, "pgy_zone_channel_create_Int", "int32_t", 1,
             ABI_FIELD_SCALAR("handle", pgy_abi_zone_channel_handle)),
    ABI_TYPE("WorldChannel<Int>", 4, 4, "pgy_world_channel_create_Int", "int32_t", 1,
             ABI_FIELD_SCALAR("handle", pgy_abi_world_channel_handle)),
    ABI_TYPE("ZoneChannel<String>", 4, 4, "pgy_zone_channel_create_String", "char*", 1,
             ABI_FIELD_SCALAR("handle", pgy_abi_zone_channel_handle)),

    /* Box<T> */
    ABI_TYPE("Box<Int>", 8, 8, "pgy_box_new_Int", "int32_t", 1,
             ABI_FIELD_STRUCT("ptr", pgy_abi_box_int, ptr)),
    ABI_TYPE("Box<String>", 8, 8, "pgy_box_new_String", "char*", 1,
             ABI_FIELD_STRUCT("ptr", pgy_abi_box_string, ptr)),

    /* Array<T> */
    ABI_TYPE("Array<Int>", 24, 8, "pgy_array_new_Int", "int32_t", 3,
             ABI_FIELD_STRUCT("data", pgy_abi_array_int, data),
             ABI_FIELD_STRUCT("len", pgy_abi_array_int, len),
             ABI_FIELD_STRUCT("cap", pgy_abi_array_int, cap)),
    ABI_TYPE("Array<String>", 24, 8, "pgy_array_new_String", "char*", 3,
             ABI_FIELD_STRUCT("data", pgy_abi_array_string, data),
             ABI_FIELD_STRUCT("len", pgy_abi_array_string, len),
             ABI_FIELD_STRUCT("cap", pgy_abi_array_string, cap)),

    /* Auxiliary */
    ABI_TYPE("Future", 8, 4, "pgy_spawn", "int32_t", 2,
             ABI_FIELD_STRUCT("handle", pgy_abi_future, handle),
             ABI_FIELD_STRUCT("ready", pgy_abi_future, ready)),
    ABI_TYPE("RemoteFuture", 24, 8, "pgy_spawn", "int32_t", 4,
             ABI_FIELD_STRUCT("handle", pgy_abi_remote_future, handle),
             ABI_FIELD_STRUCT("ready", pgy_abi_remote_future, ready),
             ABI_FIELD_STRUCT("trace_id", pgy_abi_remote_future, trace_id),
             ABI_FIELD_STRUCT("trace_data", pgy_abi_remote_future, trace_data)),
    ABI_TYPE("Qubit", 12, 4, "ClaimQubit", "int32_t", 3,
             ABI_FIELD_STRUCT("state", pgy_abi_qubit, state),
             ABI_FIELD_STRUCT("pool_id", pgy_abi_qubit, pool_id),
             ABI_FIELD_STRUCT("measured", pgy_abi_qubit, measured)),
    ABI_TYPE("TaskHandle", 8, 4, NULL, "int32_t", 2,
             ABI_FIELD_STRUCT("id", pgy_abi_task_handle, id),
             ABI_FIELD_STRUCT("valid", pgy_abi_task_handle, valid)),
    ABI_TYPE("Timer", 12, 4, NULL, "int32_t", 3,
             ABI_FIELD_STRUCT("duration", pgy_abi_timer, duration),
             ABI_FIELD_STRUCT("remaining", pgy_abi_timer, remaining),
             ABI_FIELD_STRUCT("done", pgy_abi_timer, done)),
    ABI_TYPE("Arena", 24, 8, NULL, "char*", 3,
             ABI_FIELD_STRUCT("buffer", pgy_abi_arena, buffer),
             ABI_FIELD_STRUCT("capacity", pgy_abi_arena, capacity),
             ABI_FIELD_STRUCT("offset", pgy_abi_arena, offset)),
    ABI_TYPE("Allocator", 48, 8, NULL, "void*", 8,
             ABI_FIELD_STRUCT("kind", pgy_abi_allocator, kind),
             ABI_FIELD_STRUCT("trace_enabled", pgy_abi_allocator, trace_enabled),
             ABI_FIELD_STRUCT("debug_enabled", pgy_abi_allocator, debug_enabled),
             ABI_FIELD_STRUCT("allocations", pgy_abi_allocator, allocations),
             ABI_FIELD_STRUCT("deallocations", pgy_abi_allocator, deallocations),
             ABI_FIELD_STRUCT("bytes_in_use", pgy_abi_allocator, bytes_in_use),
             ABI_FIELD_STRUCT("peak_bytes", pgy_abi_allocator, peak_bytes),
             ABI_FIELD_STRUCT("pool", pgy_abi_allocator, pool)),
};

#define PGY_ABI_TYPE_COUNT \
    (sizeof(k_abi_type_table) / sizeof(k_abi_type_table[0]))

#undef ABI_TYPE
#undef ABI_FIELD_STRUCT
#undef ABI_FIELD_SCALAR

static const MIRTypeLayout *
abi_type_lookup_by_name(const char *pergyra_type_name)
{
    if (pergyra_type_name == NULL)
        return NULL;

    for (size_t i = 0; i < PGY_ABI_TYPE_COUNT; i++) {
        if (k_abi_type_table[i].abi_type_name != NULL
            && strcmp(k_abi_type_table[i].abi_type_name, pergyra_type_name) == 0) {
            return &k_abi_type_table[i];
        }
    }
    return NULL;
}

static const MIRTypeLayout *
abi_type_lookup_by_runtime_fn(const char *runtime_fn)
{
    if (runtime_fn == NULL)
        return NULL;

    for (size_t i = 0; i < PGY_ABI_TYPE_COUNT; i++) {
        if (k_abi_type_table[i].runtime_fn != NULL
            && strcmp(k_abi_type_table[i].runtime_fn, runtime_fn) == 0) {
            return &k_abi_type_table[i];
        }
    }
    return NULL;
}

static char *
mir_abi_format_owned(const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int needed;
    int written;
    char *result;

    if (fmt == NULL)
        return NULL;

    va_start(args, fmt);
    va_copy(copy, args);
    needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(args);
        return NULL;
    }

    result = (char *)malloc((size_t)needed + 1);
    if (result == NULL) {
        va_end(args);
        return NULL;
    }
    written = vsnprintf(result, (size_t)needed + 1, fmt, args);
    va_end(args);
    if (written < 0 || written != needed) {
        free(result);
        return NULL;
    }
    return result;
}

static const MIRTypeLayout *
mir_abi_lookup_runtime_fmt(const char *fmt, const char *suffix)
{
    const MIRTypeLayout *layout;
    char *runtime_name;

    runtime_name = mir_abi_format_owned(fmt, suffix);
    if (runtime_name == NULL)
        return NULL;
    layout = abi_type_lookup_by_runtime_fn(runtime_name);
    free(runtime_name);
    return layout;
}

/* Extract inner type from "Slot<Int>" -> "Int". */
static char *
mir_extract_inner_type_suffix_owned(const char *pergyra_type_name)
{
    const char *lt;
    const char *gt;
    size_t len;
    char *suffix;

    if (pergyra_type_name == NULL)
        return NULL;
    lt = strchr(pergyra_type_name, '<');
    gt = strrchr(pergyra_type_name, '>');
    if (lt == NULL || gt == NULL || gt <= lt || gt[1] != '\0')
        return NULL;
    len = (size_t)(gt - lt - 1);
    if (len == (size_t)-1)
        return NULL;

    suffix = (char *)malloc(len + 1);
    if (suffix == NULL)
        return NULL;
    memcpy(suffix, lt + 1, len);
    suffix[len] = '\0';
    return suffix;
}

/* Lookup ABI type by Pergyra type name.
 *
 * Mode selection:
 *   1. Exact match for the given name (covers canonical names like "Slot<Int>"
 *      and non-generic types like "Future", "Qubit", etc.)
 *   2. Fall back to runtime function name pattern matching for canonical
 *      generic types.
 *   3. Fall back to exact match for non-generic auxiliary types.
 */
const MIRTypeLayout *
mir_abi_lookup(const char *pergyra_type_name)
{
    if (pergyra_type_name == NULL)
        return NULL;

    /* Step 1: Exact match (canonical names and non-generic types) */
    const MIRTypeLayout *t = abi_type_lookup_by_name(pergyra_type_name);
    if (t != NULL)
        return t;

    /* Try to find by runtime function name pattern */
    /* e.g. "Slot<Int>" -> look for any type with "pgy_claim_Int" */
    char *suffix = mir_extract_inner_type_suffix_owned(pergyra_type_name);
    if (suffix != NULL) {
        /* Check if it's a Slot type */
        if (strncmp(pergyra_type_name, "Slot<", 5) == 0) {
            t = mir_abi_lookup_runtime_fmt("pgy_claim_%s", suffix);
            if (t != NULL) {
                free(suffix);
                return t;
            }
        }
        /* Option */
        else if (strncmp(pergyra_type_name, "Option<", 7) == 0) {
            t = mir_abi_lookup_runtime_fmt("pgy_option_some_%s", suffix);
            if (t != NULL) {
                free(suffix);
                return t;
            }
        }
        /* Result */
        else if (strncmp(pergyra_type_name, "Result<", 7) == 0) {
            t = mir_abi_lookup_runtime_fmt("pgy_result_ok_%s", suffix);
            if (t != NULL) {
                free(suffix);
                return t;
            }
        }
        /* ZoneChannel<T> */
        else if (strncmp(pergyra_type_name, "ZoneChannel<", 12) == 0) {
            t = mir_abi_lookup_runtime_fmt("pgy_zone_channel_create_%s", suffix);
            if (t != NULL) {
                free(suffix);
                return t;
            }
        }
        /* WorldChannel<T> */
        else if (strncmp(pergyra_type_name, "WorldChannel<", 13) == 0) {
            t = mir_abi_lookup_runtime_fmt("pgy_world_channel_create_%s", suffix);
            if (t != NULL) {
                free(suffix);
                return t;
            }
        }
        /* Box<T> */
        else if (strncmp(pergyra_type_name, "Box<", 4) == 0) {
            t = mir_abi_lookup_runtime_fmt("pgy_box_new_%s", suffix);
            if (t != NULL) {
                free(suffix);
                return t;
            }
        }
        /* Array<T> */
        else if (strncmp(pergyra_type_name, "Array<", 6) == 0) {
            t = mir_abi_lookup_runtime_fmt("pgy_array_new_%s", suffix);
            if (t != NULL) {
                free(suffix);
                return t;
            }
        }
        free(suffix);
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
    /* ABI layouts are immutable compile-time data. Keep this compatibility
     * entrypoint so existing MIR lowering call sites do not own table setup. */
}
