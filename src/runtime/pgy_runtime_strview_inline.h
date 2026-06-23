/*
 * Copyright (c) 2025 Pergyra Language Project
 *
 * pgy_runtime_strview_inline.h -- non-owning string views (zero-allocation).
 *
 * A String at runtime is a NUL-terminated `char *`, so `Substring` must malloc a
 * copy: a pointer into the middle of a string is not independently NUL-
 * terminated. A `PgyStrView` instead borrows the source buffer as `{ptr, len}`,
 * exactly like the array `PgySlice_T { data; length; }` convention. This is the
 * Pergyra counterpart of Rust's `&str`: substring/scan operations that consume
 * the result immediately can avoid the per-call allocation that is the one
 * structural gap in tight Substring+IndexOf-style loops. The view never owns
 * or frees the buffer.
 *
 * This header is self-contained (string.h only) so it can be unit-benchmarked
 * and reused by the fused string-window builtins. The beta stdlib exposes the
 * source-level `StrView` wrapper in stdlib/strview.pgy; this C primitive remains
 * the backend/runtime owner for the no-allocation range operations.
 */
#ifndef PGY_RUNTIME_STRVIEW_INLINE_H
#define PGY_RUNTIME_STRVIEW_INLINE_H

#include <stdint.h>
#include <string.h>

typedef struct {
    const char *data;   /* borrowed; NOT NUL-terminated */
    int32_t     length;
} PgyStrView;

/* A view over s[start .. start+len), with the SAME clamping as Substring():
 * out-of-range / non-positive len yields an empty view. No allocation. */
static inline PgyStrView
pgy_strview(const char *s, int32_t start, int32_t len)
{
    PgyStrView v;
    v.data = "";
    v.length = 0;
    if (s == NULL)
        return v;
    size_t raw = strlen(s);
    if (raw > (size_t)INT32_MAX)
        return v;
    int32_t slen = (int32_t)raw;
    if (start < 0 || start >= slen || len <= 0)
        return v;
    if (len > slen - start)
        len = slen - start;
    v.data = s + start;
    v.length = len;
    return v;
}

/* Same contract as pgy_strview(), but consumes a caller-owned source-length
 * fact. This is the hot-path form for StrView/CharAtN-style code where the
 * length was already computed by the same owner and should not be re-scanned. */
static inline PgyStrView
pgy_strview_with_len(const char *s, int32_t source_len,
                     int32_t start, int32_t len)
{
    PgyStrView v;
    v.data = "";
    v.length = 0;
    if (s == NULL || source_len < 0)
        return v;
    if (start < 0 || start >= source_len || len <= 0)
        return v;
    if (len > source_len - start)
        len = source_len - start;
    v.data = s + start;
    v.length = len;
    return v;
}

static inline int32_t
pgy_strview_len(PgyStrView v)
{
    return v.length;
}

/* Index of `needle` within the view, or -1, matching StringIndexOf semantics
 * (empty needle -> 0). Searches only the view's range; no allocation. */
static inline int32_t
pgy_strview_indexof(PgyStrView v, const char *needle)
{
    if (needle == NULL)
        return -1;
    size_t nl = strlen(needle);
    if (nl == 0)
        return 0;
    if (v.length < 0 || (size_t)v.length < nl)
        return -1;
    if (nl == 1) {
        const void *match = memchr(v.data, (unsigned char)needle[0],
                                   (size_t)v.length);
        return match != NULL ? (int32_t)((const char *)match - v.data) : -1;
    }
    int32_t limit = v.length - (int32_t)nl;
    for (int32_t i = 0; i <= limit; i++) {
        if (memcmp(v.data + i, needle, nl) == 0)
            return i;
    }
    return -1;
}

static inline bool
pgy_strview_equals(PgyStrView v, const char *other)
{
    size_t n;

    if (other == NULL)
        return false;
    n = strlen(other);
    if (v.length < 0 || (size_t)v.length != n)
        return false;
    switch (n) {
    case 0:
        return true;
    case 1:
        return v.data[0] == other[0];
    case 2:
        return v.data[0] == other[0] && v.data[1] == other[1];
    case 3:
        return v.data[0] == other[0] && v.data[1] == other[1]
            && v.data[2] == other[2];
    case 4:
        return v.data[0] == other[0] && v.data[1] == other[1]
            && v.data[2] == other[2] && v.data[3] == other[3];
    default:
        break;
    }
    return memcmp(v.data, other, n) == 0;
}

static inline bool
pgy_strview_starts_with(const char *s, int32_t start, const char *prefix)
{
    size_t raw_len, prefix_len;
    int32_t slen;

    if (s == NULL || prefix == NULL)
        return false;
    raw_len = strlen(s);
    if (raw_len > (size_t)INT32_MAX)
        return false;
    slen = (int32_t)raw_len;
    if (start < 0 || start > slen)
        return false;
    prefix_len = strlen(prefix);
    if (prefix_len == 0)
        return true;
    if ((size_t)(slen - start) < prefix_len)
        return false;
    return memcmp(s + start, prefix, prefix_len) == 0;
}

/* Materialize a view into an owned NUL-terminated string when one is genuinely
 * needed (escape hatch back to the `char *` String world). Allocates. */
static inline char *
pgy_strview_to_string(PgyStrView v, char *(*alloc_fn)(size_t))
{
    int32_t n = v.length < 0 ? 0 : v.length;
    char *r = alloc_fn((size_t)n + 1);
    if (r == NULL)
        return NULL;
    if (n > 0)
        memcpy(r, v.data, (size_t)n);
    r[n] = '\0';
    return r;
}

#endif /* PGY_RUNTIME_STRVIEW_INLINE_H */
