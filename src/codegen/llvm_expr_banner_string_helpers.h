#ifndef PERGYRA_LLVM_EXPR_BANNER_STRING_HELPERS_H
#define PERGYRA_LLVM_EXPR_BANNER_STRING_HELPERS_H

static size_t
llvm_count_banner_line_indent(const char *line_start, const char *line_end)
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
llvm_line_is_empty_with_only_ws(const char *line_start, const char *line_end)
{
    for (const char *p = line_start; p < line_end; p++) {
        if (*p != ' ' && *p != '\t')
            return false;
    }
    return true;
}

static char *
llvm_normalize_banner_string_literal_scratch(const char *src, PgyArena *arena)
{
    const char *cursor = src;
    const char *end;
    size_t min_indent = 0;
    bool found_content_line = false;
    char *output = NULL;
    size_t output_cap = 0;
    size_t output_len = 0;

    if (src == NULL)
        return NULL;

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

        if (!llvm_line_is_empty_with_only_ws(line, line_end)) {
            size_t indent = llvm_count_banner_line_indent(line, line_end);
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

    output_cap = (size_t)(end - cursor + 1);
    if (output_cap == 0)
        output_cap = 1;
    output = pgy_arena_alloc(arena, output_cap);
    if (output == NULL)
        return NULL;

    for (const char *line = cursor; line < end; ) {
        const char *line_end = line;
        while (line_end < end && *line_end != '\n' && *line_end != '\r')
            line_end++;

        const char *body_start = line;
        if (llvm_line_is_empty_with_only_ws(line, line_end)) {
            body_start = line_end;
        } else {
            for (size_t i = 0; i < min_indent && body_start < line_end; i++) {
                if (*body_start == ' ' || *body_start == '\t')
                    body_start++;
                else
                    break;
            }
        }

        if (line_end > body_start) {
            size_t seg_len = (size_t)(line_end - body_start);
            if (output_len + seg_len + 2 > output_cap) {
                size_t needed = output_len + seg_len + 2;
                size_t new_cap = (output_cap * 2 > needed)
                                 ? output_cap * 2
                                 : needed;
                char *grown = pgy_arena_alloc(arena, new_cap);
                if (grown == NULL)
                    break;
                memcpy(grown, output, output_len);
                output = grown;
                output_cap = new_cap;
            }
            memcpy(output + output_len, body_start, seg_len);
            output_len += seg_len;
        }

        if (line_end < end) {
            if (output_len + 2 > output_cap) {
                size_t new_cap = output_cap * 2;
                char *grown = pgy_arena_alloc(arena, new_cap);
                if (grown == NULL)
                    break;
                memcpy(grown, output, output_len);
                output = grown;
                output_cap = new_cap;
            }
            output[output_len++] = '\n';
            if (*line_end == '\r' && line_end + 1 < end && *(line_end + 1) == '\n')
                line = line_end + 2;
            else
                line = line_end + 1;
        } else {
            break;
        }
    }

    if (output_len >= output_cap) {
        char *grown = pgy_arena_alloc(arena, output_len + 1);
        if (grown != NULL) {
            memcpy(grown, output, output_len);
            output = grown;
            output_cap = output_len + 1;
        }
    }
    if (output == NULL)
        return NULL;

    output[output_len] = '\0';
    return output;
}

#endif /* PERGYRA_LLVM_EXPR_BANNER_STRING_HELPERS_H */
