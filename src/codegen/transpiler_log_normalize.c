/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend LogBanner string normalization.
 */

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "transpiler.h"
#include "transpiler_log_normalize.h"
#include "../common/string_compat.h"

static size_t
count_banner_line_indent(const char *line_start, const char *line_end)
{
    size_t indent = 0;

    while (line_start < line_end) {
        if (*line_start == ' ' || *line_start == '\t') {
            indent++;
            line_start++;
            continue;
        }
        break;
    }
    return indent;
}

static bool
line_is_empty_with_only_ws(const char *line_start, const char *line_end)
{
    for (const char *p = line_start; p < line_end; p++) {
        if (*p != ' ' && *p != '\t')
            return false;
    }
    return true;
}

char *
normalize_banner_string_literal(const char *src)
{
    const char *cursor = src;
    const char *end;
    size_t min_indent = 0;
    bool found_content_line = false;
    CodeBuf *buf;

    if (src == NULL)
        return pergyra_strdup("");

    if (cursor[0] == '\n')
        cursor++;
    else if (cursor[0] == '\r')
        cursor += (cursor[1] == '\n') ? 2 : 1;

    end = cursor + strlen(cursor);
    while (end > cursor && (end[-1] == '\n' || end[-1] == '\r'))
        end--;

    for (const char *line = cursor; line < end; ) {
        const char *line_end = line;
        while (line_end < end && *line_end != '\n' && *line_end != '\r')
            line_end++;

        if (!line_is_empty_with_only_ws(line, line_end)) {
            size_t indent = count_banner_line_indent(line, line_end);
            if (!found_content_line || indent < min_indent) {
                min_indent = indent;
                found_content_line = true;
            }
        }

        if (line_end == end)
            break;
        if (*line_end == '\r' && line_end + 1 < end && *(line_end + 1) == '\n')
            line = line_end + 2;
        else
            line = line_end + 1;
    }

    if (!found_content_line)
        min_indent = 0;

    buf = codebuf_create();
    if (buf == NULL)
        return pergyra_strdup("");

    for (const char *line = cursor; line < end; ) {
        const char *line_end = line;
        while (line_end < end && *line_end != '\n' && *line_end != '\r')
            line_end++;

        const char *body_start = line;
        if (line_is_empty_with_only_ws(line, line_end)) {
            body_start = line_end;
        } else {
            for (size_t i = 0; i < min_indent && body_start < line_end; i++) {
                if (*body_start == ' ' || *body_start == '\t')
                    body_start++;
                else
                    break;
            }
        }

        if (line_end > body_start)
            codebuf_write_raw(buf, body_start, (size_t)(line_end - body_start));

        if (line_end < end) {
            codebuf_write(buf, "\n");
            if (*line_end == '\r' && line_end + 1 < end && *(line_end + 1) == '\n')
                line = line_end + 2;
            else
                line = line_end + 1;
        } else {
            break;
        }
    }

    char *normalized = buf->data != NULL
        ? pergyra_strdup(buf->data)
        : pergyra_strdup("");
    codebuf_destroy(buf);
    return normalized;
}
