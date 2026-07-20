#include "mir_abi_layout.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Runtime function spelling is payload carried by resource ABI rows. */
#define ABI_RESOURCE_OP(type_name, op_name, runtime_fn, call_shape) \
    { "native-resource", (type_name), (op_name), (runtime_fn), \
      "function", "mir_abi_resource_row", (call_shape), 0 }

#define ABI_RESOURCE_OPS(type_name, claim_fn, read_fn, write_fn, release_fn, \
                         claim_shape, read_shape, write_shape, release_shape) \
    ABI_RESOURCE_OP((type_name), "Claim", (claim_fn), (claim_shape)), \
    ABI_RESOURCE_OP((type_name), "Read", (read_fn), (read_shape)), \
    ABI_RESOURCE_OP((type_name), "Release", (release_fn), (release_shape)), \
    ABI_RESOURCE_OP((type_name), "Write", (write_fn), (write_shape))

#define ABI_PIN_OPS(type_name, pin_read_fn, pin_write_fn, pin_read_init_fn, \
                    pin_write_init_fn, unpin_fn, unpin_cleanup_fn, pin_shape, \
                    init_shape, unpin_shape) \
    ABI_RESOURCE_OP((type_name), "PinRead", (pin_read_fn), (pin_shape)), \
    ABI_RESOURCE_OP((type_name), "PinWrite", (pin_write_fn), (pin_shape)), \
    ABI_RESOURCE_OP((type_name), "PinReadInit", (pin_read_init_fn), \
                    (init_shape)), \
    ABI_RESOURCE_OP((type_name), "PinWriteInit", (pin_write_init_fn), \
                    (init_shape)), \
    ABI_RESOURCE_OP((type_name), "Unpin", (unpin_fn), (unpin_shape)), \
    ABI_RESOURCE_OP((type_name), "UnpinCleanup", (unpin_cleanup_fn), \
                    (unpin_shape))

#define ABI_PLAIN_RESOURCE_OPS(type_name, claim_fn, read_fn, write_fn, release_fn) \
    ABI_RESOURCE_OPS((type_name), (claim_fn), (read_fn), (write_fn), (release_fn), \
                     "returns_container", "container_ptr_to_value", \
                     "container_ptr_value_to_void", "container_ptr_to_void")

#define ABI_SECURE_RESOURCE_OPS(type_name, claim_fn, read_fn, write_fn, release_fn) \
    ABI_RESOURCE_OPS((type_name), (claim_fn), (read_fn), (write_fn), (release_fn), \
                     "token_ptr_to_container", \
                     "container_ptr_token_ptr_to_value", \
                     "container_ptr_value_token_ptr_to_void", \
                     "container_ptr_token_ptr_to_void")

#define ABI_PLAIN_PIN_OPS(type_name, pin_read_fn, pin_write_fn, pin_read_init_fn, \
                          pin_write_init_fn, unpin_fn, unpin_cleanup_fn) \
    ABI_PIN_OPS((type_name), (pin_read_fn), (pin_write_fn), \
                (pin_read_init_fn), (pin_write_init_fn), (unpin_fn), \
                (unpin_cleanup_fn), "container_ptr_to_pinned_view", \
                "pinned_view_ptr_container_ptr_to_void", \
                "pinned_view_ptr_to_void")

#define ABI_SECURE_PIN_OPS(type_name, pin_read_fn, pin_write_fn, pin_read_init_fn, \
                           pin_write_init_fn, unpin_fn, unpin_cleanup_fn) \
    ABI_PIN_OPS((type_name), (pin_read_fn), (pin_write_fn), \
                (pin_read_init_fn), (pin_write_init_fn), (unpin_fn), \
                (unpin_cleanup_fn), \
                "container_ptr_token_ptr_to_pinned_view", \
                "pinned_view_ptr_container_ptr_token_ptr_to_void", \
                "pinned_view_ptr_to_void")

static const MIRResourceRuntimeRow k_abi_resource_runtime_fn_table[] = {
    ABI_PLAIN_RESOURCE_OPS("Slot<Int>", "pgy_claim_Int", "pgy_read_Int",
                     "pgy_write_Int", "pgy_release_Int"),
    ABI_PLAIN_RESOURCE_OPS("Slot<Long>", "pgy_claim_Long", "pgy_read_Long",
                     "pgy_write_Long", "pgy_release_Long"),
    ABI_PLAIN_RESOURCE_OPS("Slot<Float>", "pgy_claim_Float", "pgy_read_Float",
                     "pgy_write_Float", "pgy_release_Float"),
    ABI_PLAIN_RESOURCE_OPS("Slot<Double>", "pgy_claim_Double", "pgy_read_Double",
                     "pgy_write_Double", "pgy_release_Double"),
    ABI_PLAIN_RESOURCE_OPS("Slot<Bool>", "pgy_claim_Bool", "pgy_read_Bool",
                     "pgy_write_Bool", "pgy_release_Bool"),
    ABI_PLAIN_RESOURCE_OPS("Slot<String>", "pgy_claim_String", "pgy_read_String",
                     "pgy_write_String", "pgy_release_String"),

    ABI_SECURE_RESOURCE_OPS("SecureSlot<Int>", "pgy_claim_secure_Int",
                     "pgy_secure_read_Int", "pgy_secure_write_Int",
                     "pgy_secure_release_Int"),
    ABI_SECURE_RESOURCE_OPS("SecureSlot<Long>", "pgy_claim_secure_Long",
                     "pgy_secure_read_Long", "pgy_secure_write_Long",
                     "pgy_secure_release_Long"),
    ABI_SECURE_RESOURCE_OPS("SecureSlot<Float>", "pgy_claim_secure_Float",
                     "pgy_secure_read_Float", "pgy_secure_write_Float",
                     "pgy_secure_release_Float"),
    ABI_SECURE_RESOURCE_OPS("SecureSlot<Double>", "pgy_claim_secure_Double",
                     "pgy_secure_read_Double", "pgy_secure_write_Double",
                     "pgy_secure_release_Double"),
    ABI_SECURE_RESOURCE_OPS("SecureSlot<Bool>", "pgy_claim_secure_Bool",
                     "pgy_secure_read_Bool", "pgy_secure_write_Bool",
                     "pgy_secure_release_Bool"),
    ABI_SECURE_RESOURCE_OPS("SecureSlot<String>", "pgy_claim_secure_String",
                     "pgy_secure_read_String", "pgy_secure_write_String",
                     "pgy_secure_release_String"),

    ABI_PLAIN_PIN_OPS("Slot<Int>", "pgy_pin_read_Int", "pgy_pin_write_Int",
                "pgy_pin_read_init_Int", "pgy_pin_write_init_Int",
                "pgy_unpin_Int", "pgy_unpin_cleanup_Int"),
    ABI_PLAIN_PIN_OPS("Slot<Long>", "pgy_pin_read_Long", "pgy_pin_write_Long",
                "pgy_pin_read_init_Long", "pgy_pin_write_init_Long",
                "pgy_unpin_Long", "pgy_unpin_cleanup_Long"),
    ABI_PLAIN_PIN_OPS("Slot<Float>", "pgy_pin_read_Float", "pgy_pin_write_Float",
                "pgy_pin_read_init_Float", "pgy_pin_write_init_Float",
                "pgy_unpin_Float", "pgy_unpin_cleanup_Float"),
    ABI_PLAIN_PIN_OPS("Slot<Double>", "pgy_pin_read_Double", "pgy_pin_write_Double",
                "pgy_pin_read_init_Double", "pgy_pin_write_init_Double",
                "pgy_unpin_Double", "pgy_unpin_cleanup_Double"),
    ABI_PLAIN_PIN_OPS("Slot<Bool>", "pgy_pin_read_Bool", "pgy_pin_write_Bool",
                "pgy_pin_read_init_Bool", "pgy_pin_write_init_Bool",
                "pgy_unpin_Bool", "pgy_unpin_cleanup_Bool"),
    ABI_PLAIN_PIN_OPS("Slot<String>", "pgy_pin_read_String", "pgy_pin_write_String",
                "pgy_pin_read_init_String", "pgy_pin_write_init_String",
                "pgy_unpin_String", "pgy_unpin_cleanup_String"),

    ABI_SECURE_PIN_OPS("SecureSlot<Int>", "pgy_secure_pin_read_Int",
                "pgy_secure_pin_write_Int", "pgy_secure_pin_read_init_Int",
                "pgy_secure_pin_write_init_Int", "pgy_secure_unpin_Int",
                "pgy_secure_unpin_cleanup_Int"),
    ABI_SECURE_PIN_OPS("SecureSlot<Long>", "pgy_secure_pin_read_Long",
                "pgy_secure_pin_write_Long", "pgy_secure_pin_read_init_Long",
                "pgy_secure_pin_write_init_Long", "pgy_secure_unpin_Long",
                "pgy_secure_unpin_cleanup_Long"),
    ABI_SECURE_PIN_OPS("SecureSlot<Float>", "pgy_secure_pin_read_Float",
                "pgy_secure_pin_write_Float", "pgy_secure_pin_read_init_Float",
                "pgy_secure_pin_write_init_Float", "pgy_secure_unpin_Float",
                "pgy_secure_unpin_cleanup_Float"),
    ABI_SECURE_PIN_OPS("SecureSlot<Double>", "pgy_secure_pin_read_Double",
                "pgy_secure_pin_write_Double", "pgy_secure_pin_read_init_Double",
                "pgy_secure_pin_write_init_Double", "pgy_secure_unpin_Double",
                "pgy_secure_unpin_cleanup_Double"),
    ABI_SECURE_PIN_OPS("SecureSlot<Bool>", "pgy_secure_pin_read_Bool",
                "pgy_secure_pin_write_Bool", "pgy_secure_pin_read_init_Bool",
                "pgy_secure_pin_write_init_Bool", "pgy_secure_unpin_Bool",
                "pgy_secure_unpin_cleanup_Bool"),
    ABI_SECURE_PIN_OPS("SecureSlot<String>", "pgy_secure_pin_read_String",
                "pgy_secure_pin_write_String", "pgy_secure_pin_read_init_String",
                "pgy_secure_pin_write_init_String", "pgy_secure_unpin_String",
                "pgy_secure_unpin_cleanup_String"),

    ABI_PLAIN_RESOURCE_OPS("DeviceSlot<Int>", "pgy_claim_device_Int",
                     "pgy_device_read_Int", "pgy_device_write_Int",
                     "pgy_release_device_Int"),
    ABI_PLAIN_RESOURCE_OPS("DeviceSlot<Long>", "pgy_claim_device_Long",
                     "pgy_device_read_Long", "pgy_device_write_Long",
                     "pgy_release_device_Long"),
    ABI_PLAIN_RESOURCE_OPS("DeviceSlot<Float>", "pgy_claim_device_Float",
                     "pgy_device_read_Float", "pgy_device_write_Float",
                     "pgy_release_device_Float"),
    ABI_PLAIN_RESOURCE_OPS("DeviceSlot<Double>", "pgy_claim_device_Double",
                     "pgy_device_read_Double", "pgy_device_write_Double",
                     "pgy_release_device_Double"),
    ABI_PLAIN_RESOURCE_OPS("DeviceSlot<Bool>", "pgy_claim_device_Bool",
                     "pgy_device_read_Bool", "pgy_device_write_Bool",
                     "pgy_release_device_Bool"),
    ABI_PLAIN_RESOURCE_OPS("DeviceSlot<String>", "pgy_claim_device_String",
                     "pgy_device_read_String", "pgy_device_write_String",
                     "pgy_release_device_String"),

    ABI_RESOURCE_OP("DeviceSlot<Int>", "SubmitRead",
                    "pgy_submit_device_read_Int",
                    "container_ptr_to_task_handle"),
    ABI_RESOURCE_OP("DeviceSlot<Long>", "SubmitRead",
                    "pgy_submit_device_read_Long",
                    "container_ptr_to_task_handle"),
    ABI_RESOURCE_OP("DeviceSlot<Float>", "SubmitRead",
                    "pgy_submit_device_read_Float",
                    "container_ptr_to_task_handle"),
    ABI_RESOURCE_OP("DeviceSlot<Double>", "SubmitRead",
                    "pgy_submit_device_read_Double",
                    "container_ptr_to_task_handle"),
    ABI_RESOURCE_OP("DeviceSlot<Bool>", "SubmitRead",
                    "pgy_submit_device_read_Bool",
                    "container_ptr_to_task_handle"),
    ABI_RESOURCE_OP("DeviceSlot<String>", "SubmitRead",
                    "pgy_submit_device_read_String",
                    "container_ptr_to_task_handle"),
};

#define PGY_ABI_RESOURCE_RUNTIME_FN_COUNT \
    (sizeof(k_abi_resource_runtime_fn_table) / sizeof(k_abi_resource_runtime_fn_table[0]))

/* Runtime-call identities are logical owner handles, not table positions.
 * Keep the small deterministic hash deliberately target-neutral: a symbol
 * spelling or materialization policy may change without changing the logical
 * operation identity.  The reserved high range keeps these IDs distinct from
 * the legacy intent-observability ABI IDs. */
#define ABI_RUNTIME_CALL_ID_MODULUS UINT64_C(0x10000000)
#define ABI_RUNTIME_CALL_ID_BASE UINT32_C(0x40000000)
static uint64_t
abi_runtime_call_id_mix_text(uint64_t hash, const char *text)
{
    if (text == NULL)
        return (hash * UINT64_C(131) + UINT64_C(1))
            % ABI_RUNTIME_CALL_ID_MODULUS;
    while (*text != '\0') {
        hash = (hash * UINT64_C(131) + (unsigned char)*text)
            % ABI_RUNTIME_CALL_ID_MODULUS;
        text++;
    }
    return hash;
}

#undef ABI_RESOURCE_OPS
#undef ABI_PIN_OPS
#undef ABI_PLAIN_RESOURCE_OPS
#undef ABI_SECURE_RESOURCE_OPS
#undef ABI_PLAIN_PIN_OPS
#undef ABI_SECURE_PIN_OPS
#undef ABI_RESOURCE_OP

size_t
mir_abi_resource_runtime_row_count(void)
{
    return PGY_ABI_RESOURCE_RUNTIME_FN_COUNT;
}

const MIRResourceRuntimeRow *
mir_abi_resource_runtime_row_at(size_t index)
{
    if (index >= PGY_ABI_RESOURCE_RUNTIME_FN_COUNT)
        return NULL;
    return &k_abi_resource_runtime_fn_table[index];
}

uint32_t
mir_abi_resource_runtime_row_id(const MIRResourceRuntimeRow *row)
{
    uint64_t hash = UINT64_C(7);

    if (row == NULL || row->domain == NULL || row->abi_type_name == NULL
        || row->resource_op_name == NULL || row->domain[0] == '\0'
        || row->abi_type_name[0] == '\0' || row->resource_op_name[0] == '\0') {
        return 0;
    }
    hash = abi_runtime_call_id_mix_text(hash, row->domain);
    hash = abi_runtime_call_id_mix_text(hash, "|");
    hash = abi_runtime_call_id_mix_text(hash, row->abi_type_name);
    hash = abi_runtime_call_id_mix_text(hash, "|");
    hash = abi_runtime_call_id_mix_text(hash, row->resource_op_name);
    return ABI_RUNTIME_CALL_ID_BASE
        + (uint32_t)(hash % ABI_RUNTIME_CALL_ID_MODULUS);
}

bool
mir_abi_resource_runtime_row_is_constructed_nominal(
    const MIRResourceRuntimeRow *row)
{
    return row != NULL
        && row->domain != NULL
        && row->materialization != NULL
        && strcmp(row->domain, "constructed-resource") == 0
        && strcmp(row->materialization,
                  "constructed_resource_runtime_spelling") == 0;
}

const MIRResourceRuntimeRow *
mir_abi_resource_runtime_row_by_type_name(const char *abi_type_name,
                                          const char *resource_op_name)
{
    if (abi_type_name == NULL || resource_op_name == NULL)
        return NULL;

    for (size_t i = 0; i < PGY_ABI_RESOURCE_RUNTIME_FN_COUNT; i++) {
        const MIRResourceRuntimeRow *row = &k_abi_resource_runtime_fn_table[i];
        if (strcmp(row->abi_type_name, abi_type_name) == 0 &&
            strcmp(row->resource_op_name, resource_op_name) == 0) {
            return row;
        }
    }
    return NULL;
}

const MIRResourceRuntimeRow *
mir_abi_resource_runtime_row_for_type_name(const char *abi_type_name,
                                           const char *resource_op_name)
{
    const char *prefix = NULL;
    MIRResourceAbiKind kind;
    size_t prefix_len;
    size_t type_len;
    size_t inner_len;
    char inner_type_name[128];
    const MIRResourceRuntimeRow *row;

    if (abi_type_name == NULL || resource_op_name == NULL)
        return NULL;

    row = mir_abi_resource_runtime_row_by_type_name(
        abi_type_name, resource_op_name);
    if (row != NULL)
        return row;

    if (strncmp(abi_type_name, "Slot<", 5) == 0) {
        prefix = "Slot<";
        kind = MIR_RESOURCE_ABI_SLOT;
    } else if (strncmp(abi_type_name, "SecureSlot<", 11) == 0) {
        prefix = "SecureSlot<";
        kind = MIR_RESOURCE_ABI_SECURE_SLOT;
    } else if (strncmp(abi_type_name, "DeviceSlot<", 11) == 0) {
        prefix = "DeviceSlot<";
        kind = MIR_RESOURCE_ABI_DEVICE_SLOT;
    } else {
        return NULL;
    }

    prefix_len = strlen(prefix);
    type_len = strlen(abi_type_name);
    if (type_len <= prefix_len + 1
        || abi_type_name[type_len - 1] != '>') {
        return NULL;
    }
    inner_len = type_len - prefix_len - 1;
    if (inner_len == 0 || inner_len >= sizeof(inner_type_name))
        return NULL;
    memcpy(inner_type_name, abi_type_name + prefix_len, inner_len);
    inner_type_name[inner_len] = '\0';
    return mir_abi_resource_runtime_row_by_kind(
        kind, inner_type_name, resource_op_name);
}

bool
mir_abi_resource_runtime_row_matches_owner(
    const MIRResourceRuntimeRow *row)
{
    const MIRResourceRuntimeRow *expected;

    if (row == NULL || row->abi_type_name == NULL
        || row->resource_op_name == NULL || row->domain == NULL
        || row->runtime_fn == NULL || row->target_kind == NULL
        || row->materialization == NULL || row->call_shape == NULL
        || row->runtime_call_abi_id == 0
        || row->runtime_call_abi_id != mir_abi_resource_runtime_row_id(row)) {
        return false;
    }
    expected = mir_abi_resource_runtime_row_for_type_name(
        row->abi_type_name, row->resource_op_name);
    return expected != NULL
        && expected->domain != NULL
        && expected->runtime_fn != NULL
        && expected->target_kind != NULL
        && expected->materialization != NULL
        && expected->call_shape != NULL
        && strcmp(row->domain, expected->domain) == 0
        && strcmp(row->runtime_fn, expected->runtime_fn) == 0
        && strcmp(row->target_kind, expected->target_kind) == 0
        && strcmp(row->materialization, expected->materialization) == 0
        && strcmp(row->call_shape, expected->call_shape) == 0;
}

const char *
mir_abi_resource_runtime_row_domain(size_t index)
{
    if (index >= PGY_ABI_RESOURCE_RUNTIME_FN_COUNT)
        return NULL;
    return k_abi_resource_runtime_fn_table[index].domain;
}

const char *
mir_abi_resource_runtime_row_type_name(size_t index)
{
    if (index >= PGY_ABI_RESOURCE_RUNTIME_FN_COUNT)
        return NULL;
    return k_abi_resource_runtime_fn_table[index].abi_type_name;
}

const char *
mir_abi_resource_runtime_row_operation(size_t index)
{
    if (index >= PGY_ABI_RESOURCE_RUNTIME_FN_COUNT)
        return NULL;
    return k_abi_resource_runtime_fn_table[index].resource_op_name;
}

const char *
mir_abi_resource_runtime_row_symbol(size_t index)
{
    if (index >= PGY_ABI_RESOURCE_RUNTIME_FN_COUNT)
        return NULL;
    return k_abi_resource_runtime_fn_table[index].runtime_fn;
}

const char *
mir_abi_resource_runtime_row_target_kind(size_t index)
{
    if (index >= PGY_ABI_RESOURCE_RUNTIME_FN_COUNT)
        return NULL;
    return k_abi_resource_runtime_fn_table[index].target_kind;
}

const char *
mir_abi_resource_runtime_row_materialization(size_t index)
{
    if (index >= PGY_ABI_RESOURCE_RUNTIME_FN_COUNT)
        return NULL;
    return k_abi_resource_runtime_fn_table[index].materialization;
}

const char *
mir_abi_resource_runtime_row_call_shape(size_t index)
{
    if (index >= PGY_ABI_RESOURCE_RUNTIME_FN_COUNT)
        return NULL;
    return k_abi_resource_runtime_fn_table[index].call_shape;
}

const char *
mir_abi_resource_runtime_fn(const MIRTypeLayout *layout,
                            const char *resource_op_name)
{
    const MIRResourceRuntimeRow *row;

    if (layout == NULL || layout->abi_type_name == NULL ||
        resource_op_name == NULL)
        return NULL;

    row = mir_abi_resource_runtime_row_by_type_name(layout->abi_type_name,
                                                    resource_op_name);
    return row != NULL ? row->runtime_fn : NULL;
}

const char *
mir_abi_resource_runtime_fn_by_type_name(const char *abi_type_name,
                                         const char *resource_op_name)
{
    return mir_abi_resource_runtime_fn(mir_abi_lookup(abi_type_name),
                                      resource_op_name);
}

const char *
mir_abi_resource_runtime_fn_by_kind(MIRResourceAbiKind kind,
                                    const char *inner_type_name,
                                    const char *resource_op_name)
{
    const MIRResourceRuntimeRow *row;

    if (inner_type_name == NULL || resource_op_name == NULL)
        return NULL;

    row = mir_abi_resource_runtime_row_by_kind(kind, inner_type_name,
                                               resource_op_name);
    return row != NULL ? row->runtime_fn : NULL;
}
