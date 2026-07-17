#include "pgy_runtime_linkage.h"
/*
 * Copyright (c) 2025 Pergyra Language Project
 *
 * pgy_runtime_string_window_inline.h -- O(1) string-window access primitives.
 *
 * Substring()/StringLength() each strlen the whole string, so per-character
 * access is O(n) and a tight scan loop (a lexer) is O(n^2). CharAtN takes the
 * caller-precomputed length, so a single character read is O(1). Kept in its own
 * small header so the (size-capped) io/qubit runtime header does not grow.
 */
#ifndef PGY_RUNTIME_STRING_WINDOW_INLINE_H
#define PGY_RUNTIME_STRING_WINDOW_INLINE_H

#include <stdint.h>
#include <stdlib.h>

/* The 1-char string s[i..i+1) in O(1) (caller passes the length; no strlen).
 * Out-of-range yields "". Allocates the 1-char result; self-contained. */
PGY_RT_DECL char *
CharAtN(const char *s, int32_t len, int32_t i)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    char *r;
    if (s != NULL && i >= 0 && i < len) {
        r = (char *)malloc(2);
        if (r != NULL) {
            r[0] = s[i];
            r[1] = '\0';
            return r;
        }
    }
    r = (char *)malloc(1);
    if (r != NULL)
        r[0] = '\0';
    return r;
}
#else
;
#endif


/* The byte at s[i] as an int (0..255) in O(1) with a caller-supplied length;
 * -1 when out of range. The allocation-free counterpart of CharAtN for hot
 * char-by-char scanning (a lexer): no per-character heap string. */
PGY_RT_DECL int32_t
CharCode(const char *s, int32_t len, int32_t i)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (s == NULL || i < 0 || i >= len)
        return -1;
    return (int32_t)(unsigned char)s[i];
}
#else
;
#endif


#endif /* PGY_RUNTIME_STRING_WINDOW_INLINE_H */
