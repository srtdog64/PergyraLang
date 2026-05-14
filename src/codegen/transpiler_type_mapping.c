#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"

#include "codegen_slot_type_policy.h"
#include "transpiler.h"
#include "transpiler_type_mapping.h"

static bool
transpiler_type_name_join(char *out, size_t out_size, const char *prefix,
                          const char *suffix)
{
    int written;

    if (out == NULL || out_size == 0 || prefix == NULL || suffix == NULL)
        return false;

    written = snprintf(out, out_size, "%s%s", prefix, suffix);
    if (written < 0 || (size_t)written >= out_size) {
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
        return "int32_t";
    mapped = transpiler_lookup_type_name_map(name, primitive_maps,
        sizeof(primitive_maps) / sizeof(primitive_maps[0]));
    if (mapped != NULL)
        return mapped;
    if (name[0] >= 'A' && name[0] <= 'Z' && name[1] == '\0')
        return "void*";
    return name;
}

static bool
slot_inner_type_name_write(const char *slot_type_name,
                           char *out,
                           size_t out_size)
{
    const char *open;
    const char *close;
    size_t len;

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';
    if (slot_type_name == NULL)
        return false;
    open = strchr(slot_type_name, '<');
    if (open == NULL) {
        copy_capped_string(out, out_size, slot_type_name);
        return out[0] != '\0';
    }
    open++;

    close = strrchr(slot_type_name, '>');
    if (close == NULL || close <= open)
        return false;

    len = (size_t)(close - open);
    if (len >= out_size)
        len = out_size - 1;
    memcpy(out, open, len);
    out[len] = '\0';
    return out[0] != '\0';
}

bool
slot_inner_type_name_copy(const char *slot_type_name,
                          char *out,
                          size_t out_size)
{
    if (out == NULL || out_size == 0)
        return false;
    if (!slot_inner_type_name_write(slot_type_name, out, out_size)) {
        copy_capped_string(out, out_size, "Unknown");
        return false;
    }
    return true;
}

void
sanitize_c_suffix(const char *type_name, char *buf, size_t buf_size)
{
    size_t out = 0;
    bool last_was_underscore = false;

    if (buf_size == 0)
        return;

    if (type_name == NULL || *type_name == '\0') {
        copy_capped_string(buf, buf_size, "Int");
        return;
    }

    for (size_t i = 0; type_name[i] != '\0' && out + 1 < buf_size; i++) {
        unsigned char ch = (unsigned char)type_name[i];
        if ((ch >= 'a' && ch <= 'z')
            || (ch >= 'A' && ch <= 'Z')
            || (ch >= '0' && ch <= '9')) {
            buf[out++] = (char)ch;
            last_was_underscore = false;
        } else {
            if (out == 0 || last_was_underscore)
                continue;
            buf[out++] = '_';
            last_was_underscore = true;
        }
    }
    while (out > 0 && buf[out - 1] == '_')
        out--;
    if (out == 0) {
        copy_capped_string(buf, buf_size, "Int");
        return;
    }
    if (buf[0] >= '0' && buf[0] <= '9') {
        if (out + 2 >= buf_size)
            out = buf_size > 3 ? buf_size - 3 : 0;
        if (out > 0)
            memmove(buf + 2, buf, out);
        buf[0] = 'T';
        buf[1] = '_';
        out += 2;
    }
    buf[out] = '\0';
}

void
copy_capped_string(char *dst, size_t dst_size, const char *src)
{
    size_t len;

    if (dst == NULL || dst_size == 0)
        return;

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    len = strlen(src);
    if (len >= dst_size)
        len = dst_size - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static bool
constructed_arg_name_write(const char *type_name, int arg_index,
                           char *buf, size_t buf_size)
{
    const char *lt;
    int current = 0;
    int depth = 0;
    size_t out = 0;

    if (buf == NULL || buf_size == 0)
        return false;
    buf[0] = '\0';
    if (type_name == NULL || arg_index < 0)
        return false;

    lt = strchr(type_name, '<');
    if (lt == NULL)
        return false;
    lt++;

    for (const char *p = lt; *p != '\0'; p++) {
        char ch = *p;
        if (ch == '<') {
            if (current == arg_index && out + 1 < buf_size)
                buf[out++] = ch;
            depth++;
            continue;
        }
        if (ch == '>') {
            if (depth == 0)
                break;
            if (current == arg_index && out + 1 < buf_size)
                buf[out++] = ch;
            depth--;
            continue;
        }
        if (ch == ',' && depth == 0) {
            if (current == arg_index)
                break;
            current++;
            continue;
        }
        if (current == arg_index) {
            if (!(out == 0 && ch == ' ') && out + 1 < buf_size)
                buf[out++] = ch;
        }
    }

    while (out > 0 && buf[out - 1] == ' ')
        out--;
    buf[out] = '\0';
    return out > 0;
}

void
copy_constructed_arg_name_at(const char *type_name, int arg_index,
                             char *buf, size_t buf_size)
{
    if (buf == NULL || buf_size == 0)
        return;

    if (!constructed_arg_name_write(type_name, arg_index, buf, buf_size))
        copy_capped_string(buf, buf_size, "Unknown");
}

static bool
generic_args_to_c_suffix_write(const char *inner_body,
                               char *buf,
                               size_t buf_size)
{
    size_t pos = 0;
    bool prev_was_sep = false;

    if (buf == NULL || buf_size == 0)
        return false;
    if (inner_body == NULL) {
        buf[0] = '\0';
        return false;
    }

    for (const char *p = inner_body; *p != '\0' && pos + 1 < buf_size; p++) {
        char c = *p;
        bool is_sep = (c == ',' || c == '<' || c == '>' || c == ' ' || c == '\t');
        if (is_sep) {
            if (!prev_was_sep && pos > 0) {
                buf[pos++] = '_';
                prev_was_sep = true;
            }
        } else {
            buf[pos++] = c;
            prev_was_sep = false;
        }
    }
    while (pos > 0 && buf[pos - 1] == '_')
        pos--;
    buf[pos] = '\0';
    return buf[0] != '\0';
}

bool
generic_args_to_c_suffix_copy(const char *inner_body,
                              char *out,
                              size_t out_size)
{
    if (out == NULL || out_size == 0)
        return false;
    if (!generic_args_to_c_suffix_write(inner_body, out, out_size)) {
        out[0] = '\0';
        return false;
    }
    return true;
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
constructed_arg_name_is_unknown(const char *type_name, int arg_index)
{
    char arg[128];

    copy_constructed_arg_name_at(type_name, arg_index, arg, sizeof(arg));
    return type_arg_name_is_unknown(arg);
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
pergyra_type_to_c_copy(const char *name, char *out, size_t out_size)
{
    static const TranspilerTypeNameMap legacy_maps[] = {
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

    if (name == NULL) {
        copy_capped_string(out, out_size, "int32_t");
        return true;
    }

    mapped = transpiler_lookup_type_name_map(name, legacy_maps,
        sizeof(legacy_maps) / sizeof(legacy_maps[0]));
    if (mapped != NULL) {
        copy_capped_string(out, out_size, mapped);
        return out[0] != '\0';
    }
    if (strncmp(name, "List<", 5) == 0) {
        if (constructed_single_arg_is_unknown(name))
            return false;
        slot_inner_type_name_copy(name, inner, sizeof(inner));
        sanitize_c_suffix(inner, suffix, sizeof(suffix));
        return transpiler_type_name_join(out, out_size, "PgyList_", suffix);
    }
    if (strncmp(name, "HashMap<", 8) == 0) {
        if (constructed_arg_name_is_unknown(name, 0)
            || constructed_arg_name_is_unknown(name, 1))
            return false;
        copy_constructed_arg_name_at(name, 1, value, sizeof(value));
        sanitize_c_suffix(value, suffix, sizeof(suffix));
        return transpiler_type_name_join(out, out_size, "PgyHashMap_", suffix);
    }
    if (strncmp(name, "Queue<", 6) == 0) {
        if (constructed_single_arg_is_unknown(name))
            return false;
        slot_inner_type_name_copy(name, inner, sizeof(inner));
        sanitize_c_suffix(inner, suffix, sizeof(suffix));
        return transpiler_type_name_join(out, out_size, "PgyQueue_", suffix);
    }
    if (strncmp(name, "Set<", 4) == 0) {
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
    if (strncmp(name, "RemoteFuture<", 13) == 0
        || strncmp(name, "Future<", 7) == 0) {
        copy_capped_string(out, out_size, "PgyTaskHandle");
        return out[0] != '\0';
    }
    if (strncmp(name, "Channel<", 8) == 0) {
        slot_inner_type_name_copy(name, inner, sizeof(inner));
        if (type_arg_name_is_unknown(inner))
            return false;
        return transpiler_type_name_join(out, out_size, "PgyChannel_", inner);
    }
    if (strncmp(name, "Weak<", 5) == 0) {
        slot_inner_type_name_copy(name, inner, sizeof(inner));
        if (type_arg_name_is_unknown(inner))
            return false;
        return transpiler_type_name_join(out, out_size, "PgyWeak_", inner);
    }
    if (strncmp(name, "Rc<", 3) == 0) {
        slot_inner_type_name_copy(name, inner, sizeof(inner));
        if (type_arg_name_is_unknown(inner))
            return false;
        return transpiler_type_name_join(out, out_size, "PgyRc_", inner);
    }
    if (strncmp(name, "Box<Array<", 10) == 0) {
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
    if (strncmp(name, "Box<", 4) == 0) {
        slot_inner_type_name_copy(name, inner, sizeof(inner));
        if (type_arg_name_is_unknown(inner))
            return false;
        return transpiler_type_name_join(out, out_size, "PgyBox_", inner);
    }
    if (strncmp(name, "Slice<", 6) == 0) {
        slot_inner_type_name_copy(name, inner, sizeof(inner));
        if (type_arg_name_is_unknown(inner))
            return false;
        return transpiler_type_name_join(out, out_size, "PgySlice_", inner);
    }
    if (strncmp(name, "Array<", 6) == 0) {
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
    if (strncmp(name, "Result<", 7) == 0) {
        size_t prefix_len = strlen("PgyResult_");
        slot_inner_type_name_copy(name, inner, sizeof(inner));
        if (strchr(inner, ',') == NULL) {
            if (type_arg_name_is_unknown(inner))
                return false;
            return transpiler_type_name_join(out, out_size, "PgyResult_", inner);
        }
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
    if (strncmp(name, "Option<", 7) == 0) {
        slot_inner_type_name_copy(name, inner, sizeof(inner));
        if (type_arg_name_is_unknown(inner))
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
