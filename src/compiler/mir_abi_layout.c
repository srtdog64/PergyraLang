#include "mir_abi_layout.h"

#include <stddef.h>
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
    ABI_TYPE("SecureSlot<Long>",
             sizeof(pgy_abi_secure_slot_long),
             _Alignof(pgy_abi_secure_slot_long),
             "pgy_claim_secure_Long", "int64_t", 3,
             ABI_FIELD_STRUCT("value", pgy_abi_secure_slot_long, value),
             ABI_FIELD_STRUCT("occupied", pgy_abi_secure_slot_long, occupied),
             ABI_FIELD_STRUCT("token", pgy_abi_secure_slot_long, token)),
    ABI_TYPE("SecureSlot<Float>",
             sizeof(pgy_abi_secure_slot_float),
             _Alignof(pgy_abi_secure_slot_float),
             "pgy_claim_secure_Float", "float", 3,
             ABI_FIELD_STRUCT("value", pgy_abi_secure_slot_float, value),
             ABI_FIELD_STRUCT("occupied", pgy_abi_secure_slot_float, occupied),
             ABI_FIELD_STRUCT("token", pgy_abi_secure_slot_float, token)),
    ABI_TYPE("SecureSlot<Double>",
             sizeof(pgy_abi_secure_slot_double),
             _Alignof(pgy_abi_secure_slot_double),
             "pgy_claim_secure_Double", "double", 3,
             ABI_FIELD_STRUCT("value", pgy_abi_secure_slot_double, value),
             ABI_FIELD_STRUCT("occupied", pgy_abi_secure_slot_double, occupied),
             ABI_FIELD_STRUCT("token", pgy_abi_secure_slot_double, token)),
    ABI_TYPE("SecureSlot<Bool>",
             sizeof(pgy_abi_secure_slot_bool),
             _Alignof(pgy_abi_secure_slot_bool),
             "pgy_claim_secure_Bool", "bool", 3,
             ABI_FIELD_STRUCT("value", pgy_abi_secure_slot_bool, value),
             ABI_FIELD_STRUCT("occupied", pgy_abi_secure_slot_bool, occupied),
             ABI_FIELD_STRUCT("token", pgy_abi_secure_slot_bool, token)),
    ABI_TYPE("SecureSlot<String>",
             sizeof(pgy_abi_secure_slot_string),
             _Alignof(pgy_abi_secure_slot_string),
             "pgy_claim_secure_String", "char*", 3,
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
    ABI_TYPE("DeviceSlot<Long>",
             sizeof(pgy_abi_device_slot_long),
             _Alignof(pgy_abi_device_slot_long),
             "pgy_claim_device_Long", "int64_t", 2,
             ABI_FIELD_STRUCT("value", pgy_abi_device_slot_long, value),
             ABI_FIELD_STRUCT("claimed", pgy_abi_device_slot_long, claimed)),
    ABI_TYPE("DeviceSlot<Float>",
             sizeof(pgy_abi_device_slot_float),
             _Alignof(pgy_abi_device_slot_float),
             "pgy_claim_device_Float", "float", 2,
             ABI_FIELD_STRUCT("value", pgy_abi_device_slot_float, value),
             ABI_FIELD_STRUCT("claimed", pgy_abi_device_slot_float, claimed)),
    ABI_TYPE("DeviceSlot<Double>",
             sizeof(pgy_abi_device_slot_double),
             _Alignof(pgy_abi_device_slot_double),
             "pgy_claim_device_Double", "double", 2,
             ABI_FIELD_STRUCT("value", pgy_abi_device_slot_double, value),
             ABI_FIELD_STRUCT("claimed", pgy_abi_device_slot_double, claimed)),
    ABI_TYPE("DeviceSlot<Bool>",
             sizeof(pgy_abi_device_slot_bool),
             _Alignof(pgy_abi_device_slot_bool),
             "pgy_claim_device_Bool", "bool", 2,
             ABI_FIELD_STRUCT("value", pgy_abi_device_slot_bool, value),
             ABI_FIELD_STRUCT("claimed", pgy_abi_device_slot_bool, claimed)),
    ABI_TYPE("DeviceSlot<String>",
             sizeof(pgy_abi_device_slot_string),
             _Alignof(pgy_abi_device_slot_string),
             "pgy_claim_device_String", "char*", 2,
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
    ABI_TYPE("Array<Int>",
             sizeof(pgy_abi_array_int),
             _Alignof(pgy_abi_array_int),
             "pgy_array_new_Int", "int32_t", 4,
             ABI_FIELD_STRUCT("data", pgy_abi_array_int, data),
             ABI_FIELD_STRUCT("length", pgy_abi_array_int, length),
             ABI_FIELD_STRUCT("capacity", pgy_abi_array_int, capacity),
             ABI_FIELD_STRUCT("allocator", pgy_abi_array_int, allocator)),
    ABI_TYPE("Array<Long>",
             sizeof(pgy_abi_array_long),
             _Alignof(pgy_abi_array_long),
             "pgy_array_new_Long", "int64_t", 4,
             ABI_FIELD_STRUCT("data", pgy_abi_array_long, data),
             ABI_FIELD_STRUCT("length", pgy_abi_array_long, length),
             ABI_FIELD_STRUCT("capacity", pgy_abi_array_long, capacity),
             ABI_FIELD_STRUCT("allocator", pgy_abi_array_long, allocator)),
    ABI_TYPE("Array<Float>",
             sizeof(pgy_abi_array_float),
             _Alignof(pgy_abi_array_float),
             "pgy_array_new_Float", "float", 4,
             ABI_FIELD_STRUCT("data", pgy_abi_array_float, data),
             ABI_FIELD_STRUCT("length", pgy_abi_array_float, length),
             ABI_FIELD_STRUCT("capacity", pgy_abi_array_float, capacity),
             ABI_FIELD_STRUCT("allocator", pgy_abi_array_float, allocator)),
    ABI_TYPE("Array<Double>",
             sizeof(pgy_abi_array_double),
             _Alignof(pgy_abi_array_double),
             "pgy_array_new_Double", "double", 4,
             ABI_FIELD_STRUCT("data", pgy_abi_array_double, data),
             ABI_FIELD_STRUCT("length", pgy_abi_array_double, length),
             ABI_FIELD_STRUCT("capacity", pgy_abi_array_double, capacity),
             ABI_FIELD_STRUCT("allocator", pgy_abi_array_double, allocator)),
    ABI_TYPE("Array<Bool>",
             sizeof(pgy_abi_array_bool),
             _Alignof(pgy_abi_array_bool),
             "pgy_array_new_Bool", "bool", 4,
             ABI_FIELD_STRUCT("data", pgy_abi_array_bool, data),
             ABI_FIELD_STRUCT("length", pgy_abi_array_bool, length),
             ABI_FIELD_STRUCT("capacity", pgy_abi_array_bool, capacity),
             ABI_FIELD_STRUCT("allocator", pgy_abi_array_bool, allocator)),
    ABI_TYPE("Array<String>",
             sizeof(pgy_abi_array_string),
             _Alignof(pgy_abi_array_string),
             "pgy_array_new_String", "char*", 4,
             ABI_FIELD_STRUCT("data", pgy_abi_array_string, data),
             ABI_FIELD_STRUCT("length", pgy_abi_array_string, length),
             ABI_FIELD_STRUCT("capacity", pgy_abi_array_string, capacity),
             ABI_FIELD_STRUCT("allocator", pgy_abi_array_string, allocator)),

    /* Single-consume text assembly owner. Runtime mutation is only exposed through
     * MIR-owned TextBuilder call rows. */
    ABI_TYPE("TextBuilder",
             sizeof(pgy_abi_text_builder),
             _Alignof(pgy_abi_text_builder),
             "pgy_text_builder_new_export", "char", 4,
             ABI_FIELD_STRUCT("data", pgy_abi_text_builder, data),
             ABI_FIELD_STRUCT("length", pgy_abi_text_builder, length),
             ABI_FIELD_STRUCT("capacity", pgy_abi_text_builder, capacity),
             ABI_FIELD_STRUCT("finished", pgy_abi_text_builder, finished)),

    /* Slice<T> */
    ABI_TYPE("Slice<Int>",
             sizeof(pgy_abi_slice_int),
             _Alignof(pgy_abi_slice_int),
             "pgy_slice_get_Int", "int32_t", 2,
             ABI_FIELD_STRUCT("data", pgy_abi_slice_int, data),
             ABI_FIELD_STRUCT("length", pgy_abi_slice_int, length)),
    ABI_TYPE("Slice<Long>",
             sizeof(pgy_abi_slice_long),
             _Alignof(pgy_abi_slice_long),
             "pgy_slice_get_Long", "int64_t", 2,
             ABI_FIELD_STRUCT("data", pgy_abi_slice_long, data),
             ABI_FIELD_STRUCT("length", pgy_abi_slice_long, length)),
    ABI_TYPE("Slice<Float>",
             sizeof(pgy_abi_slice_float),
             _Alignof(pgy_abi_slice_float),
             "pgy_slice_get_Float", "float", 2,
             ABI_FIELD_STRUCT("data", pgy_abi_slice_float, data),
             ABI_FIELD_STRUCT("length", pgy_abi_slice_float, length)),
    ABI_TYPE("Slice<Double>",
             sizeof(pgy_abi_slice_double),
             _Alignof(pgy_abi_slice_double),
             "pgy_slice_get_Double", "double", 2,
             ABI_FIELD_STRUCT("data", pgy_abi_slice_double, data),
             ABI_FIELD_STRUCT("length", pgy_abi_slice_double, length)),
    ABI_TYPE("Slice<Bool>",
             sizeof(pgy_abi_slice_bool),
             _Alignof(pgy_abi_slice_bool),
             "pgy_slice_get_Bool", "bool", 2,
             ABI_FIELD_STRUCT("data", pgy_abi_slice_bool, data),
             ABI_FIELD_STRUCT("length", pgy_abi_slice_bool, length)),
    ABI_TYPE("Slice<String>",
             sizeof(pgy_abi_slice_string),
             _Alignof(pgy_abi_slice_string),
             "pgy_slice_get_String", "char*", 2,
             ABI_FIELD_STRUCT("data", pgy_abi_slice_string, data),
             ABI_FIELD_STRUCT("length", pgy_abi_slice_string, length)),

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

static const MIRAbiTargetPolicy k_abi_target_policy_table[] = {
    {"selfhost-c", "cpu-c,self-hosted",
     "layout_shape,materialization_reason",
     "unsupported_shape,forbidden_loss_budget"},
};

#define PGY_ABI_TARGET_POLICY_COUNT \
    (sizeof(k_abi_target_policy_table) / sizeof(k_abi_target_policy_table[0]))

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

/* Lookup ABI type by Pergyra type name.
 *
 * Mode selection:
 *   1. Exact match for the given name (covers canonical names like "Slot<Int>"
 *      and non-generic types like "Future", "Qubit", etc.)
 *   2. Unknown names fail closed. Runtime function spelling is payload carried
 *      by the row; it is never an alternate lookup key for inventing layout.
 */
const MIRTypeLayout *
mir_abi_lookup(const char *pergyra_type_name)
{
    if (pergyra_type_name == NULL)
        return NULL;

    return abi_type_lookup_by_name(pergyra_type_name);
}

const MIRAbiTargetPolicy *
mir_abi_target_policy(const char *abi_name)
{
    if (abi_name == NULL)
        return NULL;

    for (size_t i = 0; i < PGY_ABI_TARGET_POLICY_COUNT; i++) {
        if (strcmp(k_abi_target_policy_table[i].abi_name, abi_name) == 0)
            return &k_abi_target_policy_table[i];
    }
    return NULL;
}

void
mir_abi_table_init(void)
{
    /* ABI layouts are immutable compile-time data. Keep this compatibility
     * entrypoint so existing MIR lowering call sites do not own table setup. */
}
