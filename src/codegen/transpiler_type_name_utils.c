#include <stdbool.h>
#include <string.h>

#include "transpiler_type_mapping.h"

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
