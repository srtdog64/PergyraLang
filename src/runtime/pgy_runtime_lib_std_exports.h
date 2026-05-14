#ifndef PGY_RUNTIME_LIB_STD_EXPORTS_H
#define PGY_RUNTIME_LIB_STD_EXPORTS_H

/* LLVM-linkable standard string, conversion, math, and random exports. */

char *StringJoin(PgyArray_String *arr, const char *sep)
{
    if (arr == NULL || arr->length == 0)
        return pgy_runtime_lib_strdup("");
    size_t slen = (sep != NULL) ? strlen(sep) : 0;
    size_t total = 0;
    for (size_t i = 0; i < arr->length; i++) {
        if (arr->data[i] != NULL) {
            size_t item_len = strlen(arr->data[i]);
            if (item_len > ((size_t)-1) - total)
                return pgy_runtime_lib_strdup("");
            total += item_len;
        }
        if (i > 0) {
            if (slen > ((size_t)-1) - total)
                return pgy_runtime_lib_strdup("");
            total += slen;
        }
    }
    if (total == (size_t)-1)
        return pgy_runtime_lib_strdup("");
    char *buf = (char *)malloc(total + 1);
    if (buf == NULL) return pgy_runtime_lib_strdup("");
    char *wp = buf;
    for (size_t i = 0; i < arr->length; i++) {
        if (i > 0 && slen > 0) { memcpy(wp, sep, slen); wp += slen; }
        if (arr->data[i] != NULL) {
            size_t l = strlen(arr->data[i]);
            memcpy(wp, arr->data[i], l);
            wp += l;
        }
    }
    *wp = '\0';
    return buf;
}

int32_t ToInt(const char *s)
{
    if (s == NULL) return 0;
    return (int32_t)strtol(s, NULL, 10);
}

float ToFloat(const char *s)
{
    if (s == NULL) return 0.0f;
    return strtof(s, NULL);
}

#include <math.h>

float Sqrt(float x)  { return sqrtf(x); }
float Pow(float x, float y) { return powf(x, y); }
float Floor(float x) { return floorf(x); }
float Ceil(float x)  { return ceilf(x); }

int32_t Random(int32_t max)
{
    if (max <= 0) return 0;
    return (int32_t)(rand() % max);
}

void SeedRandom(int32_t seed)
{
    srand((unsigned int)seed);
}

#endif /* PGY_RUNTIME_LIB_STD_EXPORTS_H */
