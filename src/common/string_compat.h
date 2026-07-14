#ifndef PERGYRA_STRING_COMPAT_H
#define PERGYRA_STRING_COMPAT_H

#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static inline char *
pergyra_strndup(const char *src, size_t length)
{
    char *copy;

    if (length > SIZE_MAX - 1 || (src == NULL && length > 0))
        return NULL;

    copy = malloc(length + 1);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, src, length);
    copy[length] = '\0';
    return copy;
}

static inline char *
pergyra_strdup(const char *src)
{
    if (src == NULL)
        return NULL;
    return pergyra_strndup(src, strlen(src));
}

static inline char *
pergyra_strdup_vprintf(const char *fmt, va_list args)
{
    va_list copy;
    int needed;
    int written;
    char *buffer;

    if (fmt == NULL)
        return NULL;

    va_copy(copy, args);
    needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed < 0)
        return NULL;

    buffer = malloc((size_t)needed + 1);
    if (buffer == NULL)
        return NULL;
    written = vsnprintf(buffer, (size_t)needed + 1, fmt, args);
    if (written < 0 || written != needed) {
        free(buffer);
        return NULL;
    }
    return buffer;
}

static inline char *
pergyra_strdup_printf(const char *fmt, ...)
{
    va_list args;
    char *buffer;

    va_start(args, fmt);
    buffer = pergyra_strdup_vprintf(fmt, args);
    va_end(args);
    return buffer;
}

static inline size_t
pergyra_strnlen_compat(const char *src, size_t max_len)
{
    size_t len = 0;

    if (src == NULL)
        return 0;
    while (len < max_len && src[len] != '\0')
        len++;
    return len;
}

static inline size_t
pergyra_str_append(char *dst, size_t dst_cap, const char *src)
{
    size_t used;
    size_t available;
    size_t src_len;
    size_t copy_len;

    if (dst == NULL || dst_cap == 0)
        return 0;
    used = pergyra_strnlen_compat(dst, dst_cap);
    if (used >= dst_cap) {
        dst[dst_cap - 1] = '\0';
        return dst_cap - 1;
    }
    if (src == NULL)
        return used;

    available = dst_cap - used - 1;
    src_len = strlen(src);
    copy_len = src_len < available ? src_len : available;
    if (copy_len > 0)
        memcpy(dst + used, src, copy_len);
    dst[used + copy_len] = '\0';
    return used + copy_len;
}

static inline size_t
pergyra_str_copy(char *dst, size_t dst_cap, const char *src)
{
    if (dst == NULL || dst_cap == 0)
        return 0;
    dst[0] = '\0';
    return pergyra_str_append(dst, dst_cap, src);
}

static inline size_t
pergyra_str_appendvf(char *dst, size_t dst_cap, const char *fmt, va_list args)
{
    va_list copy;
    size_t used;
    size_t available;
    int written;

    if (dst == NULL || dst_cap == 0)
        return 0;
    used = pergyra_strnlen_compat(dst, dst_cap);
    if (used >= dst_cap) {
        dst[dst_cap - 1] = '\0';
        return dst_cap - 1;
    }
    if (fmt == NULL)
        return used;

    available = dst_cap - used;
    va_copy(copy, args);
    written = vsnprintf(dst + used, available, fmt, copy);
    va_end(copy);
    if (written < 0) {
        dst[used] = '\0';
        return used;
    }
    if ((size_t)written >= available)
        return dst_cap - 1;
    return used + (size_t)written;
}

static inline size_t
pergyra_str_appendf(char *dst, size_t dst_cap, const char *fmt, ...)
{
    va_list args;
    size_t used;

    va_start(args, fmt);
    used = pergyra_str_appendvf(dst, dst_cap, fmt, args);
    va_end(args);
    return used;
}

/*
 * Stamp a fixed-capacity buffer that could not hold its formatted text.
 * `needed` is what vsnprintf reported it wanted (excluding the NUL).
 *
 * Diagnostics that own a heap string should just be heap-exact
 * (pergyra_strdup_vprintf). This exists for the sinks that still cannot --
 * a clipped message must not be indistinguishable from a whole one, because
 * a reader who cannot see the loss will debug the wrong thing. No-op when
 * the text fit.
 */
static inline void
pergyra_str_mark_clipped(char *dst, size_t dst_cap, int needed)
{
    char suffix[48];
    int suffix_len;

    if (dst == NULL || dst_cap < 8 || needed < 0)
        return;
    if ((size_t)needed < dst_cap)
        return;

    suffix_len = snprintf(suffix, sizeof(suffix), " ...[+%d bytes clipped]",
                          needed - (int)dst_cap + 1);
    if (suffix_len < 0 || (size_t)suffix_len >= dst_cap)
        return;

    memcpy(dst + dst_cap - 1 - (size_t)suffix_len, suffix, (size_t)suffix_len);
    dst[dst_cap - 1] = '\0';
}

#endif
