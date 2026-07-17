#ifndef PGY_RUNTIME_STRING_BUILTIN_INLINE_H
#define PGY_RUNTIME_STRING_BUILTIN_INLINE_H

#include "pgy_runtime_linkage.h"

#include "pgy_runtime_strview_inline.h"

/*
 * Source-level String builtins shared by C/LLVM runtime emission.
 *
 * This header intentionally keeps the public C symbol names (`StringJoin`,
 * `pgy_string_equals`, etc.) stable. `pgy_runtime_io_qubit_inline.h` owns file
 * I/O and aggregates the older runtime surface; this owner keeps string builtin
 * implementation out of that I/O boundary.
 */

PGY_RT_DECL bool
StringContains(const char *haystack, const char *needle)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (haystack == NULL || needle == NULL) return false;
    return strstr(haystack, needle) != NULL;
}
#else
;
#endif


PGY_RT_DECL int32_t
StringIndexOf(const char *haystack, const char *needle)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    const char *match;
    if (haystack == NULL || needle == NULL)
        return -1;
    match = strstr(haystack, needle);
    if (match == NULL)
        return -1;
    return (int32_t)(match - haystack);
}
#else
;
#endif


PGY_RT_DECL char *
Substring(const char *s, int32_t start, int32_t len)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    size_t raw_len;
    int32_t slen;
    if (s == NULL) return pgy_runtime_strdup("");
    raw_len = strlen(s);
    if (raw_len > (size_t)INT32_MAX)
        return pgy_runtime_strdup("");
    slen = (int32_t)raw_len;
    if (start < 0 || start >= slen || len <= 0) return pgy_runtime_strdup("");
    if (len > slen - start) len = slen - start;
    char *buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL) return pgy_runtime_strdup("");
    memcpy(buf, s + start, (size_t)len);
    buf[len] = '\0';
    return buf;
}
#else
;
#endif


PGY_RT_DECL int32_t
SubIndexOf(const char *s, int32_t start, int32_t len, const char *needle)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    return pgy_strview_indexof(pgy_strview(s, start, len), needle);
}
#else
;
#endif


PGY_RT_DECL int32_t
SubIndexOfWithLen(const char *s, int32_t source_len,
                  int32_t start, int32_t len, const char *needle)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    return pgy_strview_indexof(
        pgy_strview_with_len(s, source_len, start, len), needle);
}
#else
;
#endif


PGY_RT_DECL bool
SubEquals(const char *s, int32_t start, int32_t len, const char *other)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    return pgy_strview_equals(pgy_strview(s, start, len), other);
}
#else
;
#endif


PGY_RT_DECL bool
SubEqualsWithLen(const char *s, int32_t source_len,
                 int32_t start, int32_t len, const char *other)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    return pgy_strview_equals(
        pgy_strview_with_len(s, source_len, start, len), other);
}
#else
;
#endif


PGY_RT_DECL bool
SubContains(const char *s, int32_t start, int32_t len, const char *needle)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    return pgy_strview_indexof(pgy_strview(s, start, len), needle) >= 0;
}
#else
;
#endif


PGY_RT_DECL bool
SubContainsWithLen(const char *s, int32_t source_len,
                   int32_t start, int32_t len, const char *needle)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    return pgy_strview_indexof(
        pgy_strview_with_len(s, source_len, start, len), needle) >= 0;
}
#else
;
#endif


PGY_RT_DECL bool
SubStartsWith(const char *s, int32_t start, const char *prefix)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    return pgy_strview_starts_with(s, start, prefix);
}
#else
;
#endif


PGY_RT_DECL bool
SubStartsWithLen(const char *s, int32_t source_len,
                 int32_t start, const char *prefix)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    size_t prefix_len;
    if (s == NULL || prefix == NULL || source_len < 0)
        return false;
    if (start < 0 || start > source_len)
        return false;
    prefix_len = strlen(prefix);
    if (prefix_len == 0)
        return true;
    if (prefix_len > (size_t)INT32_MAX)
        return false;
    if ((size_t)(source_len - start) < prefix_len)
        return false;
    return pgy_strview_equals(
        pgy_strview_with_len(s, source_len, start, (int32_t)prefix_len),
        prefix);
}
#else
;
#endif


PGY_RT_DECL char *
StringReplace(const char *s, const char *old_str, const char *new_str)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (s == NULL || old_str == NULL || new_str == NULL)
        return pgy_runtime_strdup(s ? s : "");
    size_t old_len = strlen(old_str);
    size_t new_len = strlen(new_str);
    if (old_len == 0) return pgy_runtime_strdup(s);
    size_t count = 0;
    const char *p = s;
    while ((p = strstr(p, old_str)) != NULL) { count++; p += old_len; }
    size_t source_len = strlen(s);
    size_t result_len;
    if (new_len > old_len) {
        size_t delta = new_len - old_len;
        if (count > (((size_t)-1) - source_len) / delta)
            return pgy_runtime_strdup("");
        result_len = source_len + count * delta;
    } else if (new_len == old_len) {
        result_len = source_len;
    } else {
        size_t delta = old_len - new_len;
        result_len = source_len - count * delta;
    }
    char *result = (char *)malloc(result_len + 1);
    if (result == NULL) return pgy_runtime_strdup("");
    char *dst = result;
    p = s;
    while (*p) {
        if (strncmp(p, old_str, old_len) == 0) {
            memcpy(dst, new_str, new_len);
            dst += new_len;
            p += old_len;
        } else {
            *dst++ = *p++;
        }
    }
    *dst = '\0';
    return result;
}
#else
;
#endif


PGY_RT_DECL char *
StringTrim(const char *s)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (s == NULL) return pgy_runtime_strdup("");
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
                       s[len-1] == '\n' || s[len-1] == '\r'))
        len--;
    char *buf = (char *)malloc(len + 1);
    if (buf == NULL) return pgy_runtime_strdup("");
    memcpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}
#else
;
#endif


PGY_RT_DECL char *
ToUpper(const char *s)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (s == NULL) return pgy_runtime_strdup("");
    size_t len = strlen(s);
    char *buf = (char *)malloc(len + 1);
    if (buf == NULL) return pgy_runtime_strdup("");
    for (size_t i = 0; i <= len; i++)
        buf[i] = (s[i] >= 'a' && s[i] <= 'z') ? (char)(s[i] - 32) : s[i];
    return buf;
}
#else
;
#endif


PGY_RT_DECL char *
ToLower(const char *s)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (s == NULL) return pgy_runtime_strdup("");
    size_t len = strlen(s);
    char *buf = (char *)malloc(len + 1);
    if (buf == NULL) return pgy_runtime_strdup("");
    for (size_t i = 0; i <= len; i++)
        buf[i] = (s[i] >= 'A' && s[i] <= 'Z') ? (char)(s[i] + 32) : s[i];
    return buf;
}
#else
;
#endif


PGY_RT_DECL char *
StringConcat(const char *a, const char *b)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (a == NULL) a = "";
    if (b == NULL) b = "";
    size_t la = strlen(a), lb = strlen(b);
    if (la > ((size_t)-1) - lb || la + lb > ((size_t)-1) - 1)
        return pgy_runtime_strdup("");
    char *buf = (char *)malloc(la + lb + 1);
    if (buf == NULL) return pgy_runtime_strdup("");
    memcpy(buf, a, la);
    memcpy(buf + la, b, lb + 1);
    return buf;
}
#else
;
#endif


PGY_RT_DECL PgyArray_String
StringSplit(const char *s, const char *delim)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    PgyArray_String result = pgy_array_new_String(8);
    if (s == NULL || delim == NULL || delim[0] == '\0') {
        if (s != NULL)
            pgy_array_push_String(&result, pgy_runtime_strdup(s));
        return result;
    }
    size_t dlen = strlen(delim);
    const char *p = s;
    for (;;) {
        const char *found = strstr(p, delim);
        if (found == NULL) {
            pgy_array_push_String(&result, pgy_runtime_strdup(p));
            break;
        }
        size_t seg = (size_t)(found - p);
        char *part = (char *)malloc(seg + 1);
        if (part != NULL) {
            memcpy(part, p, seg);
            part[seg] = '\0';
        }
        pgy_array_push_String(&result,
            part != NULL ? part : pgy_runtime_strdup(""));
        p = found + dlen;
    }
    return result;
}
#else
;
#endif


PGY_RT_DECL char *
StringJoin(PgyArray_String *arr, const char *sep)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (arr == NULL || arr->data == NULL || arr->length == 0) {
        return pgy_runtime_strdup("");
    }
    if (sep == NULL) sep = "";
    size_t sep_len = strlen(sep);
    size_t total = 0;
    for (size_t i = 0; i < arr->length; i++) {
        if (arr->data[i]) {
            size_t sl = strlen(arr->data[i]);
            if (sl > ((size_t)-1) - total)
                return pgy_runtime_strdup("");
            total += sl;
        }
        if (i < arr->length - 1) {
            if (sep_len > ((size_t)-1) - total)
                return pgy_runtime_strdup("");
            total += sep_len;
        }
    }
    if (total == (size_t)-1)
        return pgy_runtime_strdup("");
    char *result = (char *)malloc(total + 1);
    if (result == NULL)
        return pgy_runtime_strdup("");
    size_t pos = 0;
    for (size_t i = 0; i < arr->length; i++) {
        if (arr->data[i]) {
            size_t sl = strlen(arr->data[i]);
            memcpy(result + pos, arr->data[i], sl);
            pos += sl;
        }
        if (i < arr->length - 1) {
            memcpy(result + pos, sep, sep_len);
            pos += sep_len;
        }
    }
    result[pos] = '\0';
    return result;
}
#else
;
#endif


PGY_RT_DECL bool
pgy_string_equals(const char *a, const char *b)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (a == NULL) a = "";
    if (b == NULL) b = "";
    return strcmp(a, b) == 0;
}
#else
;
#endif


#endif /* PGY_RUNTIME_STRING_BUILTIN_INLINE_H */
