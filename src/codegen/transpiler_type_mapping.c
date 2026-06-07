#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../common/worker_boundary_storage_policy.h"

#include "codegen_channel_runtime_abi.h"
#include "codegen_slot_type_policy.h"
#include "transpiler.h"
#include "transpiler_type_mapping.h"

static bool
transpiler_type_name_join_fits(int written, size_t out_size)
{
    return written >= 0 && (size_t)written < out_size;
}

static bool
transpiler_type_name_join(char *out, size_t out_size, const char *prefix,
                          const char *suffix)
{
    int written;

    if (out == NULL || out_size == 0 || prefix == NULL || suffix == NULL)
        return false;

    written = snprintf(out, out_size, "%s%s", prefix, suffix);
    if (!transpiler_type_name_join_fits(written, out_size)) {
        out[0] = '\0';
        return false;
    }
    return true;
}

typedef struct {
    const char *name;
    const char *c_name;
} TranspilerTypeNameMap;

static int
transpiler_type_name_map_compare(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const TranspilerTypeNameMap *map = (const TranspilerTypeNameMap *)entry;
    return strcmp(name, map->name);
}

static const char *
transpiler_lookup_type_name_map(const char *name,
                                const TranspilerTypeNameMap *maps,
                                size_t map_count)
{
    const TranspilerTypeNameMap *match;

    if (name == NULL || maps == NULL || map_count == 0)
        return NULL;
    match = (const TranspilerTypeNameMap *)bsearch(
        name, maps, map_count, sizeof(maps[0]),
        transpiler_type_name_map_compare);
    return match != NULL ? match->c_name : NULL;
}

const char *
pergyra_primitive_to_c(const char *name)
{
    static const TranspilerTypeNameMap primitive_maps[] = {
        {"Bool", "bool"},
        {"Double", "double"},
        {"Float", "float"},
        {"Int", "int32_t"},
        {"Long", "int64_t"},
        {"QubitSlot", "int32_t"},
        {"String", "char*"},
        {"Void", "void"},
    };
    const char *mapped;

    if (name == NULL)
        return NULL;
    mapped = transpiler_lookup_type_name_map(name, primitive_maps,
        sizeof(primitive_maps) / sizeof(primitive_maps[0]));
    if (mapped != NULL)
        return mapped;
    return name;
}

static bool
type_arg_name_is_unknown(const char *name)
{
    size_t len;

    while (name != NULL && (*name == ' ' || *name == '\t'))
        name++;
    if (name == NULL || name[0] == '\0')
        return true;
    len = strlen(name);
    while (len > 0 && (name[len - 1] == ' ' || name[len - 1] == '\t'))
        len--;
    return len == 7 && strncmp(name, "Unknown", 7) == 0;
}

static bool
type_arg_name_is_void(const char *name)
{
    size_t len;

    while (name != NULL && (*name == ' ' || *name == '\t'))
        name++;
    if (name == NULL)
        return false;
    len = strlen(name);
    while (len > 0 && (name[len - 1] == ' ' || name[len - 1] == '\t'))
        len--;
    return len == 4 && strncmp(name, "Void", 4) == 0;
}

static bool
type_name_fact_ident_char(char c)
{
    return (c >= 'A' && c <= 'Z')
        || (c >= 'a' && c <= 'z')
        || (c >= '0' && c <= '9')
        || c == '_';
}

bool
transpiler_type_name_contains_unknown_sentinel(const char *type_name)
{
    const char *hit = type_name;

    while (hit != NULL && (hit = strstr(hit, "Unknown")) != NULL) {
        char before = hit == type_name ? '\0' : hit[-1];
        char after = hit[7];

        if (!type_name_fact_ident_char(before)
            && !type_name_fact_ident_char(after)) {
            return true;
        }
        hit += 7;
    }

    return false;
}

bool
transpiler_type_name_is_concrete_fact(const char *type_name)
{
    return type_name != NULL
        && type_name[0] != '\0'
        && !transpiler_type_name_contains_unknown_sentinel(type_name);
}

static bool
constructed_arg_name_is_unknown(const char *type_name, int arg_index)
{
    char arg[128];

    copy_constructed_arg_name_at(type_name, arg_index, arg, sizeof(arg));
    return type_arg_name_is_unknown(arg);
}

static bool
constructed_arg_name_is_void(const char *type_name, int arg_index)
{
    char arg[128];

    copy_constructed_arg_name_at(type_name, arg_index, arg, sizeof(arg));
    return type_arg_name_is_void(arg);
}

static bool
constructed_single_arg_is_unknown(const char *type_name)
{
    char inner[128];

    if (!slot_inner_type_name_copy(type_name, inner, sizeof(inner)))
        return true;
    return type_arg_name_is_unknown(inner);
}

bool
transpiler_type_name_is_channel(const char *type_name)
{
    return type_name != NULL && strncmp(type_name, "Channel<", 8) == 0;
}

bool
transpiler_type_name_is_future(const char *type_name)
{
    return type_name != NULL && strncmp(type_name, "Future<", 7) == 0;
}

bool
transpiler_type_name_is_remote_future(const char *type_name)
{
    return type_name != NULL && strncmp(type_name, "RemoteFuture<", 13) == 0;
}

bool
transpiler_type_name_is_any_future(const char *type_name)
{
    return transpiler_type_name_is_future(type_name)
        || transpiler_type_name_is_remote_future(type_name);
}

bool
transpiler_type_name_is_result(const char *type_name)
{
    return type_name != NULL && strncmp(type_name, "Result<", 7) == 0;
}

bool
transpiler_type_name_is_option(const char *type_name)
{
    return type_name != NULL && strncmp(type_name, "Option<", 7) == 0;
}

bool
transpiler_type_name_is_array(const char *type_name)
{
    return type_name != NULL && strncmp(type_name, "Array<", 6) == 0;
}

bool
transpiler_type_name_is_slice(const char *type_name)
{
    return type_name != NULL && strncmp(type_name, "Slice<", 6) == 0;
}

bool
transpiler_type_name_is_array_or_slice(const char *type_name)
{
    return transpiler_type_name_is_array(type_name)
        || transpiler_type_name_is_slice(type_name);
}

bool
transpiler_type_name_is_list(const char *type_name)
{
    return type_name != NULL && strncmp(type_name, "List<", 5) == 0;
}

bool
transpiler_type_name_is_queue(const char *type_name)
{
    return type_name != NULL && strncmp(type_name, "Queue<", 6) == 0;
}

bool
transpiler_type_name_is_set(const char *type_name)
{
    return type_name != NULL && strncmp(type_name, "Set<", 4) == 0;
}

bool
transpiler_type_name_is_hashmap(const char *type_name)
{
    return type_name != NULL && strncmp(type_name, "HashMap<", 8) == 0;
}

const char *
codegen_worker_boundary_storage_kind_from_constructor_name(
    const char *constructor_name,
    bool include_channel,
    bool include_array_slice_alias)
{
    PgyWorkerBoundaryStorageKind kind =
        pgy_worker_boundary_storage_kind_from_constructor_name(
            constructor_name, include_channel, include_array_slice_alias);

    return pgy_worker_boundary_storage_kind_name(kind);
}

const char *
codegen_worker_boundary_storage_kind_from_type_name(const char *type_name,
                                                    bool include_channel)
{
    PgyWorkerBoundaryStorageKind kind =
        pgy_worker_boundary_storage_kind_from_type_name(type_name,
                                                        include_channel);

    return pgy_worker_boundary_storage_kind_name(kind);
}

bool
transpiler_type_name_is_box(const char *type_name)
{
    return type_name != NULL && strncmp(type_name, "Box<", 4) == 0;
}

bool
transpiler_type_name_is_box_array(const char *type_name)
{
    return type_name != NULL && strncmp(type_name, "Box<Array<", 10) == 0;
}

bool
transpiler_type_name_is_rc(const char *type_name)
{
    return type_name != NULL && strncmp(type_name, "Rc<", 3) == 0;
}

bool
transpiler_type_name_is_weak(const char *type_name)
{
    return type_name != NULL && strncmp(type_name, "Weak<", 5) == 0;
}

bool
pergyra_type_to_c_copy(const char *name, char *out, size_t out_size)
{
    static const TranspilerTypeNameMap builtin_alias_maps[] = {
        {"Allocator", "PgyAllocator"},
        {"Cooldown", "PgyCooldown"},
        {"Fsm", "PgyFsm"},
        {"HashMap", "PgyHashMap_Int"},
        {"HashMapStr", "PgyHashMap_String"},
        {"List", "PgyList_Int"},
        {"ListStr", "PgyList_String"},
        {"Queue", "PgyQueue_Int"},
        {"Set", "PgySet_String"},
        {"Timer", "PgyTimer"},
    };
    char suffix[96];
    char inner[128];
    char value[128];
    const char *mapped;

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';

    if (name == NULL)
        return false;

    mapped = transpiler_lookup_type_name_map(name, builtin_alias_maps,
        sizeof(builtin_alias_maps) / sizeof(builtin_alias_maps[0]));
    if (mapped != NULL) {
        copy_capped_string(out, out_size, mapped);
        return out[0] != '\0';
    }
    if (transpiler_type_name_is_list(name)) {
        if (constructed_single_arg_is_unknown(name))
            return false;
        slot_inner_type_name_copy(name, inner, sizeof(inner));
        sanitize_c_suffix(inner, suffix, sizeof(suffix));
        return transpiler_type_name_join(out, out_size, "PgyList_", suffix);
    }
    if (transpiler_type_name_is_hashmap(name)) {
        if (constructed_arg_name_is_unknown(name, 0)
            || constructed_arg_name_is_unknown(name, 1))
            return false;
        copy_constructed_arg_name_at(name, 1, value, sizeof(value));
        sanitize_c_suffix(value, suffix, sizeof(suffix));
        return transpiler_type_name_join(out, out_size, "PgyHashMap_", suffix);
    }
    if (transpiler_type_name_is_queue(name)) {
        if (constructed_single_arg_is_unknown(name))
            return false;
        slot_inner_type_name_copy(name, inner, sizeof(inner));
        sanitize_c_suffix(inner, suffix, sizeof(suffix));
        return transpiler_type_name_join(out, out_size, "PgyQueue_", suffix);
    }
    if (transpiler_type_name_is_set(name)) {
        slot_inner_type_name_copy(name, inner, sizeof(inner));
        if (type_arg_name_is_unknown(inner))
            return false;
        if (strcmp(inner, "String") == 0) {
            copy_capped_string(out, out_size, "PgySet_String");
            return out[0] != '\0';
        }
        if (strcmp(inner, "Int") == 0) {
            copy_capped_string(out, out_size, "PgySet_int");
            return out[0] != '\0';
        }
        sanitize_c_suffix(inner, suffix, sizeof(suffix));
        return transpiler_type_name_join(out, out_size, "PgySet_", suffix);
    }
    if (transpiler_type_name_is_any_future(name)) {
        copy_capped_string(out, out_size, "PgyTaskHandle");
        return out[0] != '\0';
    }
    if (transpiler_type_name_is_channel(name)) {
        const PgyChannelRuntimePayloadAbi *abi;

        slot_inner_type_name_copy(name, inner, sizeof(inner));
        if (type_arg_name_is_unknown(inner))
            return false;
        abi = pgy_channel_runtime_payload_abi_lookup(inner);
        if (abi == NULL)
            return false;
        return transpiler_type_name_join(out, out_size, "PgyChannel_",
            abi->suffix);
    }
    if (transpiler_type_name_is_weak(name)) {
        slot_inner_type_name_copy(name, inner, sizeof(inner));
        if (type_arg_name_is_unknown(inner))
            return false;
        return transpiler_type_name_join(out, out_size, "PgyWeak_", inner);
    }
    if (transpiler_type_name_is_rc(name)) {
        slot_inner_type_name_copy(name, inner, sizeof(inner));
        if (type_arg_name_is_unknown(inner))
            return false;
        return transpiler_type_name_join(out, out_size, "PgyRc_", inner);
    }
    if (transpiler_type_name_is_box_array(name)) {
        const char *body = name + 10;
        const char *close = strstr(body, ">>");
        size_t len = close != NULL ? (size_t)(close - body) : strlen(body);
        if (len >= sizeof(inner))
            len = sizeof(inner) - 1;
        memcpy(inner, body, len);
        inner[len] = '\0';
        if (type_arg_name_is_unknown(inner))
            return false;
        return transpiler_type_name_join(out, out_size, "PgyBoxArray_", inner);
    }
    if (transpiler_type_name_is_box(name)) {
        slot_inner_type_name_copy(name, inner, sizeof(inner));
        if (type_arg_name_is_unknown(inner))
            return false;
        return transpiler_type_name_join(out, out_size, "PgyBox_", inner);
    }
    if (transpiler_type_name_is_slice(name)) {
        slot_inner_type_name_copy(name, inner, sizeof(inner));
        if (type_arg_name_is_unknown(inner))
            return false;
        return transpiler_type_name_join(out, out_size, "PgySlice_", inner);
    }
    if (transpiler_type_name_is_array(name)) {
        slot_inner_type_name_copy(name, inner, sizeof(inner));
        if (type_arg_name_is_unknown(inner))
            return false;
        return transpiler_type_name_join(out, out_size, "PgyArray_", inner);
    }
    if (strncmp(name, "SecureSlot<", 11) == 0) {
        slot_inner_type_name_copy(name, inner, sizeof(inner));
        if (type_arg_name_is_unknown(inner))
            return false;
        return transpiler_type_name_join(out, out_size, "PgySecureSlot_", inner);
    }
    if (pgy_codegen_type_name_is_read_view(name)
        || pgy_codegen_type_name_is_write_view(name)) {
        slot_inner_type_name_copy(name, inner, sizeof(inner));
        if (type_arg_name_is_unknown(inner))
            return false;
        return transpiler_type_name_join(out, out_size, "PgySlot_", inner);
    }
    if (strncmp(name, "DeviceSlot<", 11) == 0) {
        slot_inner_type_name_copy(name, inner, sizeof(inner));
        if (type_arg_name_is_unknown(inner))
            return false;
        return transpiler_type_name_join(out, out_size, "PgyDeviceSlot_", inner);
    }
    if (strncmp(name, "Slot<", 5) == 0) {
        slot_inner_type_name_copy(name, inner, sizeof(inner));
        if (type_arg_name_is_unknown(inner))
            return false;
        return transpiler_type_name_join(out, out_size, "PgySlot_", inner);
    }
    if (transpiler_type_name_is_result(name)) {
        size_t prefix_len = strlen("PgyResult_");
        slot_inner_type_name_copy(name, inner, sizeof(inner));
        if (strchr(inner, ',') == NULL) {
            if (type_arg_name_is_unknown(inner)
                || type_arg_name_is_void(inner))
                return false;
            return transpiler_type_name_join(out, out_size, "PgyResult_", inner);
        }
        if (constructed_arg_name_is_void(name, 0))
            return false;
        if (constructed_arg_name_is_unknown(name, 0)
            || constructed_arg_name_is_unknown(name, 1)
            || prefix_len >= out_size)
            return false;
        memcpy(out, "PgyResult_", prefix_len);
        if (!generic_args_to_c_suffix_copy(inner,
                out + prefix_len,
                out_size - prefix_len)) {
            out[0] = '\0';
            return false;
        }
        return true;
    }
    if (transpiler_type_name_is_option(name)) {
        slot_inner_type_name_copy(name, inner, sizeof(inner));
        if (type_arg_name_is_unknown(inner)
            || type_arg_name_is_void(inner))
            return false;
        return transpiler_type_name_join(out, out_size, "PgyOption_", inner);
    }
    if (name[0] == '(') {
        size_t off;
        size_t i = 1;
        size_t n = strlen(name);
        bool first = true;

        out[0] = '\0';
        off = pergyra_str_append(out, out_size, "PgyTuple_");
        while (i < n && name[i] != ')') {
            char elem[96];
            char sane[96];
            size_t eo = 0;
            int depth = 0;
            while (i < n && (name[i] == ' ' || name[i] == '\t'))
                i++;
            while (i < n && eo + 1 < sizeof(elem)) {
                char c = name[i];
                if (depth == 0 && (c == ',' || c == ')'))
                    break;
                if (c == '<' || c == '(')
                    depth++;
                if (c == '>' || c == ')')
                    depth--;
                elem[eo++] = c;
                i++;
            }
            elem[eo] = '\0';
            while (eo > 0 && (elem[eo - 1] == ' ' || elem[eo - 1] == '\t'))
                elem[--eo] = '\0';
            sanitize_c_suffix(elem, sane, sizeof(sane));
            if (!first)
                off = pergyra_str_append(out, out_size, "_");
            off = pergyra_str_append(out, out_size, sane);
            first = false;
            if (i < n && name[i] == ',')
                i++;
        }
        (void)off;
        (void)pergyra_str_append(out, out_size, "_t");
        return out[0] != '\0';
    }

    mapped = pergyra_primitive_to_c(name);
    if (mapped == NULL)
        return false;
    if (strlen(mapped) >= out_size)
        return false;
    copy_capped_string(out, out_size, mapped);
    return mapped[0] != '\0';
}
