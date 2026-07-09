#include "mir_abi_layout.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    const char *abi_type_name;
    const char *resource_op_name;
    const char *runtime_fn;
} MIRResourceRuntimeFnRow;

/* Runtime function spelling is payload carried by resource ABI rows. */
#define ABI_RESOURCE_OP(type_name, op_name, runtime_fn) \
    { (type_name), (op_name), (runtime_fn) }

#define ABI_RESOURCE_OPS(type_name, claim_fn, read_fn, write_fn, release_fn) \
    ABI_RESOURCE_OP((type_name), "Claim", (claim_fn)), \
    ABI_RESOURCE_OP((type_name), "Read", (read_fn)), \
    ABI_RESOURCE_OP((type_name), "Release", (release_fn)), \
    ABI_RESOURCE_OP((type_name), "Write", (write_fn))

#define ABI_PIN_OPS(type_name, pin_read_fn, pin_write_fn, pin_read_init_fn, pin_write_init_fn, unpin_fn, unpin_cleanup_fn) \
    ABI_RESOURCE_OP((type_name), "PinRead", (pin_read_fn)), \
    ABI_RESOURCE_OP((type_name), "PinWrite", (pin_write_fn)), \
    ABI_RESOURCE_OP((type_name), "PinReadInit", (pin_read_init_fn)), \
    ABI_RESOURCE_OP((type_name), "PinWriteInit", (pin_write_init_fn)), \
    ABI_RESOURCE_OP((type_name), "Unpin", (unpin_fn)), \
    ABI_RESOURCE_OP((type_name), "UnpinCleanup", (unpin_cleanup_fn))

static const MIRResourceRuntimeFnRow k_abi_resource_runtime_fn_table[] = {
    ABI_RESOURCE_OPS("Slot<Int>", "pgy_claim_Int", "pgy_read_Int",
                     "pgy_write_Int", "pgy_release_Int"),
    ABI_RESOURCE_OPS("Slot<Long>", "pgy_claim_Long", "pgy_read_Long",
                     "pgy_write_Long", "pgy_release_Long"),
    ABI_RESOURCE_OPS("Slot<Float>", "pgy_claim_Float", "pgy_read_Float",
                     "pgy_write_Float", "pgy_release_Float"),
    ABI_RESOURCE_OPS("Slot<Double>", "pgy_claim_Double", "pgy_read_Double",
                     "pgy_write_Double", "pgy_release_Double"),
    ABI_RESOURCE_OPS("Slot<Bool>", "pgy_claim_Bool", "pgy_read_Bool",
                     "pgy_write_Bool", "pgy_release_Bool"),
    ABI_RESOURCE_OPS("Slot<String>", "pgy_claim_String", "pgy_read_String",
                     "pgy_write_String", "pgy_release_String"),

    ABI_RESOURCE_OPS("SecureSlot<Int>", "pgy_claim_secure_Int",
                     "pgy_secure_read_Int", "pgy_secure_write_Int",
                     "pgy_secure_release_Int"),
    ABI_RESOURCE_OPS("SecureSlot<Long>", "pgy_claim_secure_Long",
                     "pgy_secure_read_Long", "pgy_secure_write_Long",
                     "pgy_secure_release_Long"),
    ABI_RESOURCE_OPS("SecureSlot<Float>", "pgy_claim_secure_Float",
                     "pgy_secure_read_Float", "pgy_secure_write_Float",
                     "pgy_secure_release_Float"),
    ABI_RESOURCE_OPS("SecureSlot<Double>", "pgy_claim_secure_Double",
                     "pgy_secure_read_Double", "pgy_secure_write_Double",
                     "pgy_secure_release_Double"),
    ABI_RESOURCE_OPS("SecureSlot<Bool>", "pgy_claim_secure_Bool",
                     "pgy_secure_read_Bool", "pgy_secure_write_Bool",
                     "pgy_secure_release_Bool"),
    ABI_RESOURCE_OPS("SecureSlot<String>", "pgy_claim_secure_String",
                     "pgy_secure_read_String", "pgy_secure_write_String",
                     "pgy_secure_release_String"),

    ABI_PIN_OPS("Slot<Int>", "pgy_pin_read_Int", "pgy_pin_write_Int",
                "pgy_pin_read_init_Int", "pgy_pin_write_init_Int",
                "pgy_unpin_Int", "pgy_unpin_cleanup_Int"),
    ABI_PIN_OPS("Slot<Long>", "pgy_pin_read_Long", "pgy_pin_write_Long",
                "pgy_pin_read_init_Long", "pgy_pin_write_init_Long",
                "pgy_unpin_Long", "pgy_unpin_cleanup_Long"),
    ABI_PIN_OPS("Slot<Float>", "pgy_pin_read_Float", "pgy_pin_write_Float",
                "pgy_pin_read_init_Float", "pgy_pin_write_init_Float",
                "pgy_unpin_Float", "pgy_unpin_cleanup_Float"),
    ABI_PIN_OPS("Slot<Double>", "pgy_pin_read_Double", "pgy_pin_write_Double",
                "pgy_pin_read_init_Double", "pgy_pin_write_init_Double",
                "pgy_unpin_Double", "pgy_unpin_cleanup_Double"),
    ABI_PIN_OPS("Slot<Bool>", "pgy_pin_read_Bool", "pgy_pin_write_Bool",
                "pgy_pin_read_init_Bool", "pgy_pin_write_init_Bool",
                "pgy_unpin_Bool", "pgy_unpin_cleanup_Bool"),
    ABI_PIN_OPS("Slot<String>", "pgy_pin_read_String", "pgy_pin_write_String",
                "pgy_pin_read_init_String", "pgy_pin_write_init_String",
                "pgy_unpin_String", "pgy_unpin_cleanup_String"),

    ABI_PIN_OPS("SecureSlot<Int>", "pgy_secure_pin_read_Int",
                "pgy_secure_pin_write_Int", "pgy_secure_pin_read_init_Int",
                "pgy_secure_pin_write_init_Int", "pgy_secure_unpin_Int",
                "pgy_secure_unpin_cleanup_Int"),
    ABI_PIN_OPS("SecureSlot<Long>", "pgy_secure_pin_read_Long",
                "pgy_secure_pin_write_Long", "pgy_secure_pin_read_init_Long",
                "pgy_secure_pin_write_init_Long", "pgy_secure_unpin_Long",
                "pgy_secure_unpin_cleanup_Long"),
    ABI_PIN_OPS("SecureSlot<Float>", "pgy_secure_pin_read_Float",
                "pgy_secure_pin_write_Float", "pgy_secure_pin_read_init_Float",
                "pgy_secure_pin_write_init_Float", "pgy_secure_unpin_Float",
                "pgy_secure_unpin_cleanup_Float"),
    ABI_PIN_OPS("SecureSlot<Double>", "pgy_secure_pin_read_Double",
                "pgy_secure_pin_write_Double", "pgy_secure_pin_read_init_Double",
                "pgy_secure_pin_write_init_Double", "pgy_secure_unpin_Double",
                "pgy_secure_unpin_cleanup_Double"),
    ABI_PIN_OPS("SecureSlot<Bool>", "pgy_secure_pin_read_Bool",
                "pgy_secure_pin_write_Bool", "pgy_secure_pin_read_init_Bool",
                "pgy_secure_pin_write_init_Bool", "pgy_secure_unpin_Bool",
                "pgy_secure_unpin_cleanup_Bool"),
    ABI_PIN_OPS("SecureSlot<String>", "pgy_secure_pin_read_String",
                "pgy_secure_pin_write_String", "pgy_secure_pin_read_init_String",
                "pgy_secure_pin_write_init_String", "pgy_secure_unpin_String",
                "pgy_secure_unpin_cleanup_String"),

    ABI_RESOURCE_OPS("DeviceSlot<Int>", "pgy_claim_device_Int",
                     "pgy_device_read_Int", "pgy_device_write_Int",
                     "pgy_release_device_Int"),
    ABI_RESOURCE_OPS("DeviceSlot<Long>", "pgy_claim_device_Long",
                     "pgy_device_read_Long", "pgy_device_write_Long",
                     "pgy_release_device_Long"),
    ABI_RESOURCE_OPS("DeviceSlot<Float>", "pgy_claim_device_Float",
                     "pgy_device_read_Float", "pgy_device_write_Float",
                     "pgy_release_device_Float"),
    ABI_RESOURCE_OPS("DeviceSlot<Double>", "pgy_claim_device_Double",
                     "pgy_device_read_Double", "pgy_device_write_Double",
                     "pgy_release_device_Double"),
    ABI_RESOURCE_OPS("DeviceSlot<Bool>", "pgy_claim_device_Bool",
                     "pgy_device_read_Bool", "pgy_device_write_Bool",
                     "pgy_release_device_Bool"),
    ABI_RESOURCE_OPS("DeviceSlot<String>", "pgy_claim_device_String",
                     "pgy_device_read_String", "pgy_device_write_String",
                     "pgy_release_device_String"),

    ABI_RESOURCE_OP("DeviceSlot<Int>", "SubmitRead",
                    "pgy_submit_device_read_Int"),
    ABI_RESOURCE_OP("DeviceSlot<Long>", "SubmitRead",
                    "pgy_submit_device_read_Long"),
    ABI_RESOURCE_OP("DeviceSlot<Float>", "SubmitRead",
                    "pgy_submit_device_read_Float"),
    ABI_RESOURCE_OP("DeviceSlot<Double>", "SubmitRead",
                    "pgy_submit_device_read_Double"),
    ABI_RESOURCE_OP("DeviceSlot<Bool>", "SubmitRead",
                    "pgy_submit_device_read_Bool"),
    ABI_RESOURCE_OP("DeviceSlot<String>", "SubmitRead",
                    "pgy_submit_device_read_String"),
};

#define PGY_ABI_RESOURCE_RUNTIME_FN_COUNT \
    (sizeof(k_abi_resource_runtime_fn_table) / sizeof(k_abi_resource_runtime_fn_table[0]))

#undef ABI_RESOURCE_OPS
#undef ABI_PIN_OPS
#undef ABI_RESOURCE_OP

size_t
mir_abi_resource_runtime_row_count(void)
{
    return PGY_ABI_RESOURCE_RUNTIME_FN_COUNT;
}

const char *
mir_abi_resource_runtime_row_domain(size_t index)
{
    if (index >= PGY_ABI_RESOURCE_RUNTIME_FN_COUNT)
        return NULL;
    return "native-resource";
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
    return "function";
}

const char *
mir_abi_resource_runtime_row_materialization(size_t index)
{
    if (index >= PGY_ABI_RESOURCE_RUNTIME_FN_COUNT)
        return NULL;
    return "mir_abi_resource_row";
}

static const char *
mir_abi_resource_runtime_call_shape(const char *type_name,
                                    const char *resource_op_name)
{
    bool is_secure = type_name != NULL && strncmp(type_name, "SecureSlot<", 11) == 0;

    if (resource_op_name == NULL)
        return NULL;

    if (strcmp(resource_op_name, "Claim") == 0)
        return is_secure ? "token_ptr_to_container" : "returns_container";
    if (strcmp(resource_op_name, "Read") == 0)
        return is_secure ? "container_ptr_token_ptr_to_value"
                         : "container_ptr_to_value";
    if (strcmp(resource_op_name, "Release") == 0)
        return is_secure ? "container_ptr_token_ptr_to_void"
                         : "container_ptr_to_void";
    if (strcmp(resource_op_name, "Write") == 0)
        return is_secure ? "container_ptr_value_token_ptr_to_void"
                         : "container_ptr_value_to_void";
    if (strcmp(resource_op_name, "PinRead") == 0 ||
        strcmp(resource_op_name, "PinWrite") == 0)
        return is_secure ? "container_ptr_token_ptr_to_pinned_view"
                         : "container_ptr_to_pinned_view";
    if (strcmp(resource_op_name, "PinReadInit") == 0 ||
        strcmp(resource_op_name, "PinWriteInit") == 0)
        return is_secure ? "pinned_view_ptr_container_ptr_token_ptr_to_void"
                         : "pinned_view_ptr_container_ptr_to_void";
    if (strcmp(resource_op_name, "Unpin") == 0 ||
        strcmp(resource_op_name, "UnpinCleanup") == 0)
        return "pinned_view_ptr_to_void";
    if (strcmp(resource_op_name, "SubmitRead") == 0)
        return "container_ptr_to_task_handle";
    return NULL;
}

const char *
mir_abi_resource_runtime_row_call_shape(size_t index)
{
    if (index >= PGY_ABI_RESOURCE_RUNTIME_FN_COUNT)
        return NULL;
    return mir_abi_resource_runtime_call_shape(
        k_abi_resource_runtime_fn_table[index].abi_type_name,
        k_abi_resource_runtime_fn_table[index].resource_op_name);
}

static bool
abi_runtime_suffix_copy(const char *type_name, char *buf, size_t buf_size)
{
    size_t out = 0;
    size_t i = 0;
    bool last_was_underscore = false;

    if (buf == NULL || buf_size == 0)
        return false;
    buf[0] = '\0';
    if (type_name == NULL || type_name[0] == '\0')
        return false;
    if (strcmp(type_name, "Unknown") == 0 || strcmp(type_name, "Void") == 0)
        return false;

    for (i = 0; type_name[i] != '\0' && out + 1 < buf_size; i++) {
        unsigned char ch = (unsigned char)type_name[i];
        if ((ch >= 'a' && ch <= 'z')
            || (ch >= 'A' && ch <= 'Z')
            || (ch >= '0' && ch <= '9')) {
            buf[out++] = (char)ch;
            last_was_underscore = false;
        } else if (out > 0 && !last_was_underscore) {
            buf[out++] = '_';
            last_was_underscore = true;
        }
    }
    if (type_name[i] != '\0') {
        buf[0] = '\0';
        return false;
    }
    while (out > 0 && buf[out - 1] == '_')
        out--;
    if (out == 0)
        return false;
    if (buf[0] >= '0' && buf[0] <= '9') {
        if (out + 3 >= buf_size) {
            buf[0] = '\0';
            return false;
        }
        memmove(buf + 2, buf, out);
        buf[0] = 'T';
        buf[1] = '_';
        out += 2;
    }
    buf[out] = '\0';
    return true;
}

static const char *
abi_constructed_resource_runtime_prefix(MIRResourceAbiKind kind,
                                        const char *resource_op_name)
{
    if (resource_op_name == NULL)
        return NULL;

    switch (kind) {
    case MIR_RESOURCE_ABI_SLOT:
        if (strcmp(resource_op_name, "Claim") == 0)
            return "pgy_claim_";
        if (strcmp(resource_op_name, "Read") == 0)
            return "pgy_read_";
        if (strcmp(resource_op_name, "Write") == 0)
            return "pgy_write_";
        if (strcmp(resource_op_name, "Release") == 0)
            return "pgy_release_";
        if (strcmp(resource_op_name, "PinRead") == 0)
            return "pgy_pin_read_";
        if (strcmp(resource_op_name, "PinWrite") == 0)
            return "pgy_pin_write_";
        if (strcmp(resource_op_name, "PinReadInit") == 0)
            return "pgy_pin_read_init_";
        if (strcmp(resource_op_name, "PinWriteInit") == 0)
            return "pgy_pin_write_init_";
        if (strcmp(resource_op_name, "Unpin") == 0)
            return "pgy_unpin_";
        if (strcmp(resource_op_name, "UnpinCleanup") == 0)
            return "pgy_unpin_cleanup_";
        return NULL;
    case MIR_RESOURCE_ABI_SECURE_SLOT:
        if (strcmp(resource_op_name, "Claim") == 0)
            return "pgy_claim_secure_";
        if (strcmp(resource_op_name, "Read") == 0)
            return "pgy_secure_read_";
        if (strcmp(resource_op_name, "Write") == 0)
            return "pgy_secure_write_";
        if (strcmp(resource_op_name, "Release") == 0)
            return "pgy_secure_release_";
        if (strcmp(resource_op_name, "PinRead") == 0)
            return "pgy_secure_pin_read_";
        if (strcmp(resource_op_name, "PinWrite") == 0)
            return "pgy_secure_pin_write_";
        if (strcmp(resource_op_name, "PinReadInit") == 0)
            return "pgy_secure_pin_read_init_";
        if (strcmp(resource_op_name, "PinWriteInit") == 0)
            return "pgy_secure_pin_write_init_";
        if (strcmp(resource_op_name, "Unpin") == 0)
            return "pgy_secure_unpin_";
        if (strcmp(resource_op_name, "UnpinCleanup") == 0)
            return "pgy_secure_unpin_cleanup_";
        return NULL;
    case MIR_RESOURCE_ABI_DEVICE_SLOT:
        if (strcmp(resource_op_name, "Claim") == 0)
            return "pgy_claim_device_";
        if (strcmp(resource_op_name, "Read") == 0)
            return "pgy_device_read_";
        if (strcmp(resource_op_name, "Write") == 0)
            return "pgy_device_write_";
        if (strcmp(resource_op_name, "Release") == 0)
            return "pgy_release_device_";
        if (strcmp(resource_op_name, "SubmitRead") == 0)
            return "pgy_submit_device_read_";
        return NULL;
    }

    return NULL;
}

static const char *
abi_constructed_resource_runtime_fn(MIRResourceAbiKind kind,
                                    const char *inner_type_name,
                                    const char *resource_op_name)
{
    enum { ABI_RUNTIME_FN_RING_SIZE = 16, ABI_RUNTIME_FN_BUF_SIZE = 160 };
    static _Thread_local char buffers[ABI_RUNTIME_FN_RING_SIZE][ABI_RUNTIME_FN_BUF_SIZE];
    static _Thread_local size_t next_buffer;
    const char *prefix;
    char suffix[96];
    char *dst;
    int written;

    prefix = abi_constructed_resource_runtime_prefix(kind, resource_op_name);
    if (prefix == NULL)
        return NULL;
    if (!abi_runtime_suffix_copy(inner_type_name, suffix, sizeof(suffix)))
        return NULL;

    dst = buffers[next_buffer++ % ABI_RUNTIME_FN_RING_SIZE];
    written = snprintf(dst, ABI_RUNTIME_FN_BUF_SIZE, "%s%s", prefix, suffix);
    if (written < 0 || (size_t)written >= ABI_RUNTIME_FN_BUF_SIZE) {
        dst[0] = '\0';
        return NULL;
    }
    return dst;
}

const char *
mir_abi_resource_runtime_fn(const MIRTypeLayout *layout,
                            const char *resource_op_name)
{
    if (layout == NULL || layout->abi_type_name == NULL ||
        resource_op_name == NULL)
        return NULL;

    for (size_t i = 0; i < PGY_ABI_RESOURCE_RUNTIME_FN_COUNT; i++) {
        const MIRResourceRuntimeFnRow *row = &k_abi_resource_runtime_fn_table[i];
        if (strcmp(row->abi_type_name, layout->abi_type_name) == 0 &&
            strcmp(row->resource_op_name, resource_op_name) == 0) {
            return row->runtime_fn;
        }
    }
    return NULL;
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
    const char *container_name;
    char abi_type_name[96];
    const char *runtime_fn;
    int written;

    if (inner_type_name == NULL || resource_op_name == NULL)
        return NULL;

    switch (kind) {
    case MIR_RESOURCE_ABI_SLOT:
        container_name = "Slot";
        break;
    case MIR_RESOURCE_ABI_SECURE_SLOT:
        container_name = "SecureSlot";
        break;
    case MIR_RESOURCE_ABI_DEVICE_SLOT:
        container_name = "DeviceSlot";
        break;
    default:
        return NULL;
    }

    written = snprintf(abi_type_name, sizeof(abi_type_name), "%s<%s>",
                       container_name, inner_type_name);
    if (written < 0 || (size_t)written >= sizeof(abi_type_name))
        return NULL;

    runtime_fn = mir_abi_resource_runtime_fn_by_type_name(abi_type_name,
                                                          resource_op_name);
    if (runtime_fn != NULL)
        return runtime_fn;

    return abi_constructed_resource_runtime_fn(kind, inner_type_name,
                                               resource_op_name);
}
