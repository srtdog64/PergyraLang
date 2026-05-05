#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "codegen_slot_type_policy.h"
#include "transpiler.h"

const char *
pergyra_primitive_to_c(const char *name)
{
    if (name == NULL)
        return "int32_t";
    if (strcmp(name, "Int")    == 0) return "int32_t";
    if (strcmp(name, "Long")   == 0) return "int64_t";
    if (strcmp(name, "Float")  == 0) return "float";
    if (strcmp(name, "Double") == 0) return "double";
    if (strcmp(name, "Bool")   == 0) return "bool";
    if (strcmp(name, "String") == 0) return "char*";
    if (strcmp(name, "QubitSlot") == 0) return "int32_t";
    if (strcmp(name, "Void")   == 0) return "void";
    if (name[0] >= 'A' && name[0] <= 'Z' && name[1] == '\0')
        return "void*";
    return name;
}

const char *
slot_inner_type_name(const char *slot_type_name)
{
    const char *open;
    const char *close;
    size_t len;
    static char buf[128];

    if (slot_type_name == NULL)
        return "Unknown";
    open = strchr(slot_type_name, '<');
    if (open == NULL)
        return slot_type_name;
    open++;

    close = strrchr(slot_type_name, '>');
    if (close == NULL || close <= open)
        return "Unknown";

    len = (size_t)(close - open);
    if (len >= sizeof(buf))
        len = sizeof(buf) - 1;
    memcpy(buf, open, len);
    buf[len] = '\0';
    return buf;
}

void
sanitize_c_suffix(const char *type_name, char *buf, size_t buf_size)
{
    size_t out = 0;
    bool last_was_underscore = false;

    if (buf_size == 0)
        return;

    if (type_name == NULL || *type_name == '\0') {
        snprintf(buf, buf_size, "Int");
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
        snprintf(buf, buf_size, "Int");
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

const char *
constructed_arg_name_at(const char *type_name, int arg_index)
{
    static char buf[128];
    const char *lt;
    int current = 0;
    int depth = 0;
    size_t out = 0;

    if (type_name == NULL || arg_index < 0)
        return "Unknown";

    lt = strchr(type_name, '<');
    if (lt == NULL)
        return "Unknown";
    lt++;

    for (const char *p = lt; *p != '\0'; p++) {
        char ch = *p;
        if (ch == '<') {
            if (current == arg_index && out + 1 < sizeof(buf))
                buf[out++] = ch;
            depth++;
            continue;
        }
        if (ch == '>') {
            if (depth == 0)
                break;
            if (current == arg_index && out + 1 < sizeof(buf))
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
            if (!(out == 0 && ch == ' ') && out + 1 < sizeof(buf))
                buf[out++] = ch;
        }
    }

    while (out > 0 && buf[out - 1] == ' ')
        out--;
    buf[out] = '\0';
    return out > 0 ? buf : "Unknown";
}

void
copy_constructed_arg_name_at(const char *type_name, int arg_index,
                             char *buf, size_t buf_size)
{
    const char *arg_name;

    if (buf == NULL || buf_size == 0)
        return;

    arg_name = constructed_arg_name_at(type_name, arg_index);
    if (arg_name == NULL)
        arg_name = "Unknown";
    snprintf(buf, buf_size, "%s", arg_name);
}

const char *
generic_args_to_c_suffix(const char *inner_body)
{
    static char buf[256];
    size_t pos = 0;
    bool prev_was_sep = false;

    if (inner_body == NULL) {
        buf[0] = '\0';
        return buf;
    }

    for (const char *p = inner_body; *p != '\0' && pos + 1 < sizeof(buf); p++) {
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
    return buf;
}

const char *
pergyra_type_to_c(const char *name)
{
    static char buf[128];
    char suffix[96];

    if (name == NULL)
        return "int32_t";
    if (strcmp(name, "HashMap") == 0)
        return "PgyHashMap_Int";
    if (strcmp(name, "HashMapStr") == 0)
        return "PgyHashMap_String";
    if (strcmp(name, "List") == 0)
        return "PgyList_Int";
    if (strcmp(name, "ListStr") == 0)
        return "PgyList_String";
    if (strcmp(name, "Set") == 0)
        return "PgySet_String";
    if (strcmp(name, "Queue") == 0)
        return "PgyQueue_Int";
    if (strcmp(name, "Fsm") == 0)
        return "PgyFsm";
    if (strcmp(name, "Timer") == 0)
        return "PgyTimer";
    if (strcmp(name, "Cooldown") == 0)
        return "PgyCooldown";
    if (strcmp(name, "Allocator") == 0)
        return "PgyAllocator";
    if (strncmp(name, "List<", 5) == 0) {
        sanitize_c_suffix(slot_inner_type_name(name), suffix, sizeof(suffix));
        snprintf(buf, sizeof(buf), "PgyList_%s", suffix);
        return buf;
    }
    if (strncmp(name, "HashMap<", 8) == 0) {
        sanitize_c_suffix(constructed_arg_name_at(name, 1), suffix, sizeof(suffix));
        snprintf(buf, sizeof(buf), "PgyHashMap_%s", suffix);
        return buf;
    }
    if (strncmp(name, "Queue<", 6) == 0) {
        sanitize_c_suffix(slot_inner_type_name(name), suffix, sizeof(suffix));
        snprintf(buf, sizeof(buf), "PgyQueue_%s", suffix);
        return buf;
    }
    if (strncmp(name, "Set<", 4) == 0) {
        const char *inner = slot_inner_type_name(name);
        if (inner != NULL && strcmp(inner, "String") == 0) {
            snprintf(buf, sizeof(buf), "PgySet_String");
        } else if (inner != NULL && strcmp(inner, "Int") == 0) {
            snprintf(buf, sizeof(buf), "PgySet_int");
        } else {
            sanitize_c_suffix(inner, suffix, sizeof(suffix));
            snprintf(buf, sizeof(buf), "PgySet_%s", suffix);
        }
        return buf;
    }
    if (strncmp(name, "RemoteFuture<", 13) == 0)
        return "PgyTaskHandle";
    if (strncmp(name, "Future<", 7) == 0)
        return "PgyTaskHandle";
    if (strncmp(name, "Channel<", 8) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyChannel_%s", inner);
        return buf;
    }
    if (strncmp(name, "Weak<", 5) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyWeak_%s", inner);
        return buf;
    }
    if (strncmp(name, "Rc<", 3) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyRc_%s", inner);
        return buf;
    }
    if (strncmp(name, "Box<Array<", 10) == 0) {
        static char buf[128];
        const char *inner = name + 10;
        const char *close = strstr(inner, ">>");
        size_t len = close != NULL ? (size_t)(close - inner) : strlen(inner);
        if (len > 63) len = 63;
        char inner_buf[64];
        memcpy(inner_buf, inner, len);
        inner_buf[len] = '\0';
        snprintf(buf, sizeof(buf), "PgyBoxArray_%s", inner_buf);
        return buf;
    }
    if (strncmp(name, "Box<", 4) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyBox_%s", inner);
        return buf;
    }
    if (strncmp(name, "Slice<", 6) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgySlice_%s", inner);
        return buf;
    }
    if (strncmp(name, "Array<", 6) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyArray_%s", inner);
        return buf;
    }
    if (strncmp(name, "SecureSlot<", 11) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgySecureSlot_%s", inner);
        return buf;
    }
    if (pgy_codegen_type_name_is_read_view(name)) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgySlot_%s", inner);
        return buf;
    }
    if (pgy_codegen_type_name_is_write_view(name)) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgySlot_%s", inner);
        return buf;
    }
    if (strncmp(name, "DeviceSlot<", 11) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyDeviceSlot_%s", inner);
        return buf;
    }
    if (strncmp(name, "Slot<", 5) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgySlot_%s", inner);
        return buf;
    }
    if (strncmp(name, "Result<", 7) == 0) {
        static char buf[256];
        const char *inner = slot_inner_type_name(name);
        if (strchr(inner, ',') == NULL) {
            snprintf(buf, sizeof(buf), "PgyResult_%s", inner);
            return buf;
        }
        const char *suffix = generic_args_to_c_suffix(inner);
        size_t prefix_len = strlen("PgyResult_");
        memcpy(buf, "PgyResult_", prefix_len);
        copy_capped_string(buf + prefix_len, sizeof(buf) - prefix_len, suffix);
        return buf;
    }
    if (strncmp(name, "Option<", 7) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyOption_%s", inner);
        return buf;
    }
    if (name[0] == '(') {
        static char buf[256];
        size_t off = 0;
        size_t i = 1;
        size_t n = strlen(name);
        bool first = true;
        off += (size_t)snprintf(buf + off, sizeof(buf) - off, "PgyTuple_");
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
                if (c == '<' || c == '(') depth++;
                if (c == '>' || c == ')') depth--;
                elem[eo++] = c;
                i++;
            }
            elem[eo] = '\0';
            while (eo > 0 && (elem[eo - 1] == ' ' || elem[eo - 1] == '\t'))
                elem[--eo] = '\0';
            sanitize_c_suffix(elem, sane, sizeof(sane));
            if (!first)
                off += (size_t)snprintf(buf + off, sizeof(buf) - off, "_");
            off += (size_t)snprintf(buf + off, sizeof(buf) - off, "%s", sane);
            first = false;
            if (i < n && name[i] == ',') i++;
        }
        snprintf(buf + off, sizeof(buf) - off, "_t");
        return buf;
    }
    return pergyra_primitive_to_c(name);
}
