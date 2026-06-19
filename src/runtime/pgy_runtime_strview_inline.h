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
 * structural gap measured against idiomatic Rust string code (~10x on a tight
 * Substring+IndexOf loop). The view never owns or frees the buffer.
 *
 * This header is self-contained (string.h only) so it can be unit-benchmarked
 * before the language surface (a `StrView` type + view-returning builtins) is
 * wired through the type system and both backends.
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
    int32_t limit = v.length - (int32_t)nl;
    for (int32_t i = 0; i <= limit; i++) {
        if (memcmp(v.data + i, needle, nl) == 0)
            return i;
    }
    return -1;
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
