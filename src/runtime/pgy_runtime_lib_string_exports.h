/* Core string ABI exports (contains/index/substring/replace/trim/case/concat);
 * included from pgy_runtime_lib_io_string_exports.h after the io owner so
 * pgy_runtime_lib_strdup is already defined. The LLVM-linkable symbol names
 * are unchanged by this split. */
bool StringContains(const char *haystack, const char *needle)
{
    if (haystack == NULL || needle == NULL)
        return false;
    return strstr(haystack, needle) != NULL;
}
int32_t StringIndexOf(const char *haystack, const char *needle)
{
    const char *match;
    if (haystack == NULL || needle == NULL)
        return -1;
    match = strstr(haystack, needle);
    if (match == NULL)
        return -1;
    return (int32_t)(match - haystack);
}
char *Substring(const char *s, int32_t start, int32_t len)
{
    size_t raw_len;
    int32_t slen;
    if (s == NULL)
        return pgy_runtime_lib_strdup("");
    raw_len = strlen(s);
    if (raw_len > (size_t)INT32_MAX)
        return pgy_runtime_lib_strdup("");
    slen = (int32_t)raw_len;
    if (start < 0 || start >= slen || len <= 0)
        return pgy_runtime_lib_strdup("");
    if (len > slen - start)
        len = slen - start;
    char *buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL)
        return pgy_runtime_lib_strdup("");
    memcpy(buf, s + start, (size_t)len);
    buf[len] = '\0';
    return buf;
}
char *SubstringWithLen(const char *s, int32_t source_len,
                       int32_t start, int32_t len)
{
    char *buf;
    if (s == NULL || source_len < 0 || start < 0 || start >= source_len ||
        len <= 0)
        return pgy_runtime_lib_strdup("");
    if (len > source_len - start)
        len = source_len - start;
    buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL)
        return pgy_runtime_lib_strdup("");
    memcpy(buf, s + start, (size_t)len);
    buf[len] = '\0';
    return buf;
}
/* Allocation-free StringIndexOf(Substring(s, start, len), needle): index of
 * `needle` within s[start .. start+len) relative to `start`, or -1. */
int32_t SubIndexOf(const char *s, int32_t start, int32_t len, const char *needle)
{
    return pgy_strview_indexof(pgy_strview(s, start, len), needle);
}
int32_t SubIndexOfWithLen(const char *s, int32_t source_len,
                          int32_t start, int32_t len, const char *needle)
{
    return pgy_strview_indexof(
        pgy_strview_with_len(s, source_len, start, len), needle);
}
/* Allocation-free Substring(s, start, len) == other. */
bool SubEquals(const char *s, int32_t start, int32_t len, const char *other)
{
    return pgy_strview_equals(pgy_strview(s, start, len), other);
}
bool SubEqualsWithLen(const char *s, int32_t source_len,
                      int32_t start, int32_t len, const char *other)
{
    return pgy_strview_equals(
        pgy_strview_with_len(s, source_len, start, len), other);
}
/* Allocation-free: `needle` occurs within s[start .. start+len). */
bool SubContains(const char *s, int32_t start, int32_t len, const char *needle)
{
    return pgy_strview_indexof(pgy_strview(s, start, len), needle) >= 0;
}
bool SubContainsWithLen(const char *s, int32_t source_len,
                        int32_t start, int32_t len, const char *needle)
{
    return pgy_strview_indexof(
        pgy_strview_with_len(s, source_len, start, len), needle) >= 0;
}
/* Allocation-free: the suffix s[start..] begins with `prefix`. */
bool SubStartsWith(const char *s, int32_t start, const char *prefix)
{
    return pgy_strview_starts_with(s, start, prefix);
}
bool SubStartsWithLen(const char *s, int32_t source_len,
                      int32_t start, const char *prefix)
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
/* O(1) 1-char access with a caller-supplied length (no strlen). Out-of-range
 * yields "". See the inline header for the rationale. */
char *CharAtN(const char *s, int32_t len, int32_t i)
{
    char *r;
    if (s == NULL || i < 0 || i >= len)
        return pgy_runtime_lib_strdup("");
    r = (char *)malloc(2);
    if (r == NULL)
        return pgy_runtime_lib_strdup("");
    r[0] = s[i];
    r[1] = '\0';
    return r;
}
/* O(1) byte-at-index as an int (0..255), -1 out of range. No allocation. */
int32_t CharCode(const char *s, int32_t len, int32_t i)
{
    if (s == NULL || i < 0 || i >= len)
        return -1;
    return (int32_t)(unsigned char)s[i];
}
char *StringReplace(const char *s, const char *old_str, const char *new_str)
{
    if (s == NULL || old_str == NULL || new_str == NULL)
        return pgy_runtime_lib_strdup(s != NULL ? s : "");
    size_t old_len = strlen(old_str);
    size_t new_len = strlen(new_str);
    if (old_len == 0)
        return pgy_runtime_lib_strdup(s);
    size_t count = 0;
    const char *p = s;
    while ((p = strstr(p, old_str)) != NULL) {
        count++;
        p += old_len;
    }
    size_t source_len = strlen(s);
    size_t result_len;
    if (new_len > old_len) {
        size_t delta = new_len - old_len;
        if (count > (((size_t)-1) - source_len) / delta)
            return pgy_runtime_lib_strdup("");
        result_len = source_len + count * delta;
    } else if (new_len == old_len) {
        result_len = source_len;
    } else {
        size_t delta = old_len - new_len;
        result_len = source_len - count * delta;
    }
    char *result = (char *)malloc(result_len + 1);
    char *dst = result;
    if (result == NULL)
        return pgy_runtime_lib_strdup("");
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
char *StringTrim(const char *s)
{
    if (s == NULL)
        return pgy_runtime_lib_strdup("");
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;
    size_t len = strlen(s);
    while (len > 0
           && (s[len - 1] == ' ' || s[len - 1] == '\t'
               || s[len - 1] == '\n' || s[len - 1] == '\r'))
        len--;
    char *buf = (char *)malloc(len + 1);
    if (buf == NULL)
        return pgy_runtime_lib_strdup("");
    memcpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}
char *ToUpper(const char *s)
{
    if (s == NULL)
        return pgy_runtime_lib_strdup("");
    size_t len = strlen(s);
    char *buf = (char *)malloc(len + 1);
    if (buf == NULL)
        return pgy_runtime_lib_strdup("");
    for (size_t i = 0; i <= len; i++)
        buf[i] = (s[i] >= 'a' && s[i] <= 'z') ? (char)(s[i] - 32) : s[i];
    return buf;
}
char *ToLower(const char *s)
{
    if (s == NULL)
        return pgy_runtime_lib_strdup("");
    size_t len = strlen(s);
    char *buf = (char *)malloc(len + 1);
    if (buf == NULL)
        return pgy_runtime_lib_strdup("");
    for (size_t i = 0; i <= len; i++)
        buf[i] = (s[i] >= 'A' && s[i] <= 'Z') ? (char)(s[i] + 32) : s[i];
    return buf;
}
char *StringConcat(const char *a, const char *b)
{
    if (a == NULL)
        a = "";
    if (b == NULL)
        b = "";
    size_t la = strlen(a);
    size_t lb = strlen(b);
    if (la > ((size_t)-1) - lb || la + lb > ((size_t)-1) - 1)
        return pgy_runtime_lib_strdup("");
    char *buf = (char *)malloc(la + lb + 1);
    if (buf == NULL)
        return pgy_runtime_lib_strdup("");
    memcpy(buf, a, la);
    memcpy(buf + la, b, lb + 1);
    return buf;
}
bool pgy_string_equals(const char *a, const char *b)
{
    if (a == NULL)
        a = "";
    if (b == NULL)
        b = "";
    return strcmp(a, b) == 0;
}
#include "pgy_runtime_lib_string_split_exports.h"
