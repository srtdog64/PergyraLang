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
float Round(float x) { return roundf(x); }
float Sin(float x)   { return sinf(x); }
float Cos(float x)   { return cosf(x); }
float Tan(float x)   { return tanf(x); }
float Asin(float x)  { return asinf(x); }
float Acos(float x)  { return acosf(x); }
float Atan(float x)  { return atanf(x); }
float Atan2(float y, float x) { return atan2f(y, x); }
float Exp(float x)   { return expf(x); }
float MathLog(float x) { return logf(x); }
float Log10(float x) { return log10f(x); }
float Log2(float x)  { return log2f(x); }

int32_t Random(int32_t max)
{
    pgy_cap_require_export(PGY_CAP_RANDOM, "random");
    if (max <= 0) return 0;
    pthread_mutex_lock(&pgy_runtime_lib_rng_mutex);
    int32_t value = (int32_t)(rand() % max);
    pthread_mutex_unlock(&pgy_runtime_lib_rng_mutex);
    return value;
}

void SeedRandom(int32_t seed)
{
    pgy_cap_require_export(PGY_CAP_RANDOM, "seed-random");
    pthread_mutex_lock(&pgy_runtime_lib_rng_mutex);
    srand((unsigned int)seed);
    pthread_mutex_unlock(&pgy_runtime_lib_rng_mutex);
}

#endif /* PGY_RUNTIME_LIB_STD_EXPORTS_H */
