#include "mir_abi_layout.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const char *
mir_abi_resource_runtime_call_shape(const char *type_name,
                                    const char *resource_op_name)
{
    bool is_secure = type_name != NULL
        && strncmp(type_name, "SecureSlot<", 11) == 0;

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

static const MIRResourceRuntimeRow *
abi_constructed_resource_runtime_row(MIRResourceAbiKind kind,
                                     const char *inner_type_name,
                                     const char *resource_op_name)
{
    enum {
        ABI_RUNTIME_ROW_RING_SIZE = 16,
        ABI_RUNTIME_TYPE_BUF_SIZE = 96,
        ABI_RUNTIME_FN_BUF_SIZE = 160
    };
    static _Thread_local MIRResourceRuntimeRow rows[ABI_RUNTIME_ROW_RING_SIZE];
    static _Thread_local char type_names[ABI_RUNTIME_ROW_RING_SIZE]
                                        [ABI_RUNTIME_TYPE_BUF_SIZE];
    static _Thread_local char runtime_fns[ABI_RUNTIME_ROW_RING_SIZE]
                                        [ABI_RUNTIME_FN_BUF_SIZE];
    static _Thread_local size_t next_row;
    const char *container_name;
    const char *prefix;
    const char *call_shape;
    char suffix[96];
    size_t row_index;
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

    prefix = abi_constructed_resource_runtime_prefix(kind, resource_op_name);
    if (prefix == NULL)
        return NULL;
    if (!abi_runtime_suffix_copy(inner_type_name, suffix, sizeof(suffix)))
        return NULL;

    row_index = next_row++ % ABI_RUNTIME_ROW_RING_SIZE;
    written = snprintf(type_names[row_index], ABI_RUNTIME_TYPE_BUF_SIZE,
                       "%s<%s>", container_name, inner_type_name);
    if (written < 0 || (size_t)written >= ABI_RUNTIME_TYPE_BUF_SIZE) {
        type_names[row_index][0] = '\0';
        return NULL;
    }
    written = snprintf(runtime_fns[row_index], ABI_RUNTIME_FN_BUF_SIZE,
                       "%s%s", prefix, suffix);
    if (written < 0 || (size_t)written >= ABI_RUNTIME_FN_BUF_SIZE) {
        runtime_fns[row_index][0] = '\0';
        return NULL;
    }

    call_shape = mir_abi_resource_runtime_call_shape(type_names[row_index],
                                                     resource_op_name);
    if (call_shape == NULL)
        return NULL;

    rows[row_index] = (MIRResourceRuntimeRow) {
        "constructed-resource",
        type_names[row_index],
        resource_op_name,
        runtime_fns[row_index],
        "function",
        "constructed_resource_runtime_spelling",
        call_shape,
        0
    };
    return &rows[row_index];
}

const MIRResourceRuntimeRow *
mir_abi_resource_runtime_row_by_kind(MIRResourceAbiKind kind,
                                     const char *inner_type_name,
                                     const char *resource_op_name)
{
    const char *container_name;
    char abi_type_name[96];
    const MIRResourceRuntimeRow *row;
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

    row = mir_abi_resource_runtime_row_by_type_name(abi_type_name,
                                                    resource_op_name);
    if (row != NULL)
        return row;

    return abi_constructed_resource_runtime_row(kind, inner_type_name,
                                                resource_op_name);
}
