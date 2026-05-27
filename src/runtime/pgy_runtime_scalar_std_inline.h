/* =================================================================
 * Log — Type-safe Logging
 * ================================================================= */

#include "../common/string_compat.h"

static inline void pgy_log_int(int32_t v)    { printf("%d\n", v); }
static inline void pgy_log_long(int64_t v)   { printf("%lld\n", (long long)v); }
static inline void pgy_log_float(float v)    { printf("%f\n", v); }
static inline void pgy_log_double(double v)  { printf("%lf\n", v); }
static inline void pgy_log_bool(bool v)      { printf("%s\n", v ? "true" : "false"); }
static inline void
pgy_log_string(const char *v)
{
    const char *msg = (v == NULL ? "(null)" : v);
    fputs(msg, stdout);
    size_t msg_len = strlen(msg);
    if (msg_len == 0 || msg[msg_len - 1] != '\n')
        fputc('\n', stdout);
    fflush(stdout);
}

/* Banner/raw log helper: keep multiline payload intact and avoid truncation. */
static inline void
pgy_log_banner(const char *v)
{
    pgy_log_string(v);
}

#define pgy_log(x) _Generic((x), \
    int32_t:  pgy_log_int,    \
    int64_t:  pgy_log_long,   \
    float:    pgy_log_float,  \
    double:   pgy_log_double, \
    bool:     pgy_log_bool,   \
    char*:    pgy_log_string, \
    const char*: pgy_log_string \
)(x)

/* =================================================================
 * Math / Random Helpers (C backend inline)
 * ================================================================= */

#include <math.h>

static inline int32_t ToInt(const char *s)    { return s == NULL ? 0 : (int32_t)strtol(s, NULL, 10); }
static inline float   ToFloat(const char *s)  { return s == NULL ? 0.0f : strtof(s, NULL); }
static inline float  Sqrt(float x)            { return sqrtf(x); }
static inline float  Pow(float x, float y)    { return powf(x, y); }
static inline float  Floor(float x)           { return floorf(x); }
static inline float  Ceil(float x)            { return ceilf(x); }
static inline float  Round(float x)           { return roundf(x); }
static inline float  Sin(float x)             { return sinf(x); }
static inline float  Cos(float x)             { return cosf(x); }
static inline float  Tan(float x)             { return tanf(x); }
static inline float  Asin(float x)            { return asinf(x); }
static inline float  Acos(float x)            { return acosf(x); }
static inline float  Atan(float x)            { return atanf(x); }
static inline float  Atan2(float y, float x)  { return atan2f(y, x); }
static inline float  Exp(float x)             { return expf(x); }
static inline float  MathLog(float x)         { return logf(x); }
static inline float  Log10(float x)           { return log10f(x); }
static inline float  Log2(float x)            { return log2f(x); }
#define PGY_PI 3.14159265358979323846f
#define PGY_E  2.71828182845904523536f

static inline int32_t Clamp(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline int32_t
Random(int32_t max)
{
    if (max <= 0)
        return 0;
    pthread_mutex_lock(&pgy_runtime_rng_mutex);
    int32_t value = (int32_t)(rand() % max);
    pthread_mutex_unlock(&pgy_runtime_rng_mutex);
    return value;
}

static inline void
SeedRandom(int32_t seed)
{
    pthread_mutex_lock(&pgy_runtime_rng_mutex);
    srand((unsigned int)seed);
    pthread_mutex_unlock(&pgy_runtime_rng_mutex);
}

/* =================================================================
 * Standard Library Helpers
 * ================================================================= */

static inline char* pgy_int_to_string(int32_t val) {
    char *buf = pergyra_strdup_printf("%d", val);

    if (buf == NULL) {
        char *fallback = (char *)malloc(2);
        if (fallback != NULL) {
            fallback[0] = '0';
            fallback[1] = '\0';
        }
        return fallback;
    }
    return buf;
}

static inline char* pgy_long_to_string(int64_t val) {
    char *buf = pergyra_strdup_printf("%lld", (long long)val);
    if (buf == NULL) {
        char *fallback = (char *)malloc(2);
        if (fallback != NULL) {
            fallback[0] = '0';
            fallback[1] = '\0';
        }
        return fallback;
    }
    return buf;
}

static inline char* pgy_float_to_string(float val) {
    char *buf = pergyra_strdup_printf("%g", (double)val);
    if (buf == NULL) {
        char *fallback = (char *)malloc(4);
        if (fallback != NULL) { fallback[0] = '0'; fallback[1] = '.'; fallback[2] = '0'; fallback[3] = '\0'; }
        return fallback;
    }
    return buf;
}

static inline char* pgy_bool_to_string(bool val) {
    const char *s = val ? "true" : "false";
    size_t len = strlen(s);
    char *buf = (char *)malloc(len + 1);
    if (buf != NULL) memcpy(buf, s, len + 1);
    return buf;
}

/* Console I/O: Input(prompt), Print(msg) are already defined below as
 * pgy_input() and pgy_print(). See type_checker_builtins.c for semantic. */
