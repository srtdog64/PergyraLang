/*
 * Copyright (c) 2026 Pergyra Language Project
 * Shared C backend heap string formatting helpers.
 */

#include "transpiler.h"
#include "transpiler_format.h"

#include "../common/string_compat.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

char *
escape_c_string_literal(const char *src)
{
    size_t len = 0;
    size_t i;
    size_t j = 0;
    char *out;

    if (src == NULL)
        return pergyra_strdup("");

    for (i = 0; src[i] != '\0'; i++) {
        switch (src[i]) {
        case '\n':
        case '\r':
        case '\t':
        case '\\':
        case '"':
            len += 2;
            break;
        default:
            len += 1;
            break;
        }
    }

    out = (char *)malloc(len + 1);
    if (out == NULL)
        return pergyra_strdup("");

    for (i = 0; src[i] != '\0'; i++) {
        switch (src[i]) {
        case '\n': out[j++] = '\\'; out[j++] = 'n'; break;
        case '\r': out[j++] = '\\'; out[j++] = 'r'; break;
        case '\t': out[j++] = '\\'; out[j++] = 't'; break;
        case '\\': out[j++] = '\\'; out[j++] = '\\'; break;
        case '"': out[j++] = '\\'; out[j++] = '"'; break;
        default: out[j++] = src[i]; break;
        }
    }
    out[j] = '\0';
    return out;
}

char *
strdup_fmt(const char *fmt, ...)
{
    va_list ap;
    int n;
    char *s;

    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0)
        return pergyra_strdup("");

    s = malloc((size_t)n + 1);
    if (s == NULL)
        return pergyra_strdup("");

    va_start(ap, fmt);
    vsnprintf(s, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return s;
}
