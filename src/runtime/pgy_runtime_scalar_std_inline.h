#include "pgy_runtime_linkage.h"
/* =================================================================
 * Log — Type-safe Logging
 * ================================================================= */

#include "../common/string_compat.h"

PGY_RT_DECL void pgy_log_int(int32_t v)    
#ifndef PGY_RUNTIME_DECLS_ONLY
{ printf("%d\n", v); }
#else
;
#endif

PGY_RT_DECL void pgy_log_long(int64_t v)   
#ifndef PGY_RUNTIME_DECLS_ONLY
{ printf("%lld\n", (long long)v); }
#else
;
#endif

PGY_RT_DECL void pgy_log_float(float v)    
#ifndef PGY_RUNTIME_DECLS_ONLY
{ printf("%f\n", v); }
#else
;
#endif

PGY_RT_DECL void pgy_log_double(double v)  
#ifndef PGY_RUNTIME_DECLS_ONLY
{ printf("%lf\n", v); }
#else
;
#endif

PGY_RT_DECL void pgy_log_bool(bool v)      
#ifndef PGY_RUNTIME_DECLS_ONLY
{ printf("%s\n", v ? "true" : "false"); }
#else
;
#endif

PGY_RT_DECL void
pgy_log_string(const char *v)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    const char *msg = (v == NULL ? "(null)" : v);
    fputs(msg, stdout);
    size_t msg_len = strlen(msg);
    if (msg_len == 0 || msg[msg_len - 1] != '\n')
        fputc('\n', stdout);
    fflush(stdout);
}
#else
;
#endif


/* Banner/raw log helper: keep multiline payload intact and avoid truncation. */
PGY_RT_DECL void
pgy_log_banner(const char *v)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    pgy_log_string(v);
}
#else
;
#endif


/* Integer associations are spelled with builtin types: fixed-width typedefs
 * resolve differently per ABI (glibc LP64 makes int64_t 'long'; LLP64 and
 * Apple make it 'long long'), which left an emitted 'long long' literal
 * (e.g. 0LL) with no association on Linux. int, long, and long long are
 * formally distinct types on every ABI, so all three never collide. */
#define pgy_log(x) _Generic((x), \
    int:      pgy_log_int,     \
    long:     pgy_log_long,    \
    long long: pgy_log_long,   \
    float:    pgy_log_float,   \
    double:   pgy_log_double,  \
    bool:     pgy_log_bool,    \
    char*:    pgy_log_string,  \
    const char*: pgy_log_string \
)(x)

/* =================================================================
 * Math / Random Helpers (C backend inline)
 * ================================================================= */

#include <math.h>

PGY_RT_DECL int32_t ToInt(const char *s)    
#ifndef PGY_RUNTIME_DECLS_ONLY
{ return s == NULL ? 0 : (int32_t)strtol(s, NULL, 10); }
#else
;
#endif

PGY_RT_DECL float   ToFloat(const char *s)  
#ifndef PGY_RUNTIME_DECLS_ONLY
{ return s == NULL ? 0.0f : strtof(s, NULL); }
#else
;
#endif

PGY_RT_DECL float  Sqrt(float x)            
#ifndef PGY_RUNTIME_DECLS_ONLY
{ return sqrtf(x); }
#else
;
#endif

PGY_RT_DECL float  Pow(float x, float y)    
#ifndef PGY_RUNTIME_DECLS_ONLY
{ return powf(x, y); }
#else
;
#endif

PGY_RT_DECL float  Floor(float x)           
#ifndef PGY_RUNTIME_DECLS_ONLY
{ return floorf(x); }
#else
;
#endif

PGY_RT_DECL float  Ceil(float x)            
#ifndef PGY_RUNTIME_DECLS_ONLY
{ return ceilf(x); }
#else
;
#endif

PGY_RT_DECL float  Round(float x)           
#ifndef PGY_RUNTIME_DECLS_ONLY
{ return roundf(x); }
#else
;
#endif

PGY_RT_DECL float  Sin(float x)             
#ifndef PGY_RUNTIME_DECLS_ONLY
{ return sinf(x); }
#else
;
#endif

PGY_RT_DECL float  Cos(float x)             
#ifndef PGY_RUNTIME_DECLS_ONLY
{ return cosf(x); }
#else
;
#endif

PGY_RT_DECL float  Tan(float x)             
#ifndef PGY_RUNTIME_DECLS_ONLY
{ return tanf(x); }
#else
;
#endif

PGY_RT_DECL float  Asin(float x)            
#ifndef PGY_RUNTIME_DECLS_ONLY
{ return asinf(x); }
#else
;
#endif

PGY_RT_DECL float  Acos(float x)            
#ifndef PGY_RUNTIME_DECLS_ONLY
{ return acosf(x); }
#else
;
#endif

PGY_RT_DECL float  Atan(float x)            
#ifndef PGY_RUNTIME_DECLS_ONLY
{ return atanf(x); }
#else
;
#endif

PGY_RT_DECL float  Atan2(float y, float x)  
#ifndef PGY_RUNTIME_DECLS_ONLY
{ return atan2f(y, x); }
#else
;
#endif

PGY_RT_DECL float  Exp(float x)             
#ifndef PGY_RUNTIME_DECLS_ONLY
{ return expf(x); }
#else
;
#endif

PGY_RT_DECL float  MathLog(float x)         
#ifndef PGY_RUNTIME_DECLS_ONLY
{ return logf(x); }
#else
;
#endif

PGY_RT_DECL float  Log10(float x)           
#ifndef PGY_RUNTIME_DECLS_ONLY
{ return log10f(x); }
#else
;
#endif

PGY_RT_DECL float  Log2(float x)            
#ifndef PGY_RUNTIME_DECLS_ONLY
{ return log2f(x); }
#else
;
#endif

#define PGY_PI 3.14159265358979323846f
#define PGY_E  2.71828182845904523536f

PGY_RT_DECL int32_t Clamp(int32_t v, int32_t lo, int32_t hi) 
#ifndef PGY_RUNTIME_DECLS_ONLY
{
    return v < lo ? lo : (v > hi ? hi : v);
}
#else
;
#endif


PGY_RT_DECL int32_t
Random(int32_t max)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    pgy_cap_require_export(PGY_CAP_RANDOM, "random");
    if (max <= 0)
        return 0;
    pthread_mutex_lock(&pgy_runtime_rng_mutex);
    int32_t value = (int32_t)(rand() % max);
    pthread_mutex_unlock(&pgy_runtime_rng_mutex);
    return value;
}
#else
;
#endif


PGY_RT_DECL void
SeedRandom(int32_t seed)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    pgy_cap_require_export(PGY_CAP_RANDOM, "seed-random");
    pthread_mutex_lock(&pgy_runtime_rng_mutex);
    srand((unsigned int)seed);
    pthread_mutex_unlock(&pgy_runtime_rng_mutex);
}
#else
;
#endif


/* =================================================================
 * Standard Library Helpers
 * ================================================================= */

PGY_RT_DECL char* pgy_int_to_string(int32_t val) 
#ifndef PGY_RUNTIME_DECLS_ONLY
{
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
#else
;
#endif


PGY_RT_DECL char* pgy_long_to_string(int64_t val) 
#ifndef PGY_RUNTIME_DECLS_ONLY
{
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
#else
;
#endif


PGY_RT_DECL char* pgy_float_to_string(float val) 
#ifndef PGY_RUNTIME_DECLS_ONLY
{
    char *buf = pergyra_strdup_printf("%g", (double)val);
    if (buf == NULL) {
        char *fallback = (char *)malloc(4);
        if (fallback != NULL) { fallback[0] = '0'; fallback[1] = '.'; fallback[2] = '0'; fallback[3] = '\0'; }
        return fallback;
    }
    return buf;
}
#else
;
#endif


PGY_RT_DECL char* pgy_double_to_string(double val) 
#ifndef PGY_RUNTIME_DECLS_ONLY
{
    char *buf = pergyra_strdup_printf("%g", val);
    if (buf == NULL) {
        char *fallback = (char *)malloc(4);
        if (fallback != NULL) { fallback[0] = '0'; fallback[1] = '.'; fallback[2] = '0'; fallback[3] = '\0'; }
        return fallback;
    }
    return buf;
}
#else
;
#endif


PGY_RT_DECL char* pgy_bool_to_string(bool val) 
#ifndef PGY_RUNTIME_DECLS_ONLY
{
    const char *s = val ? "true" : "false";
    size_t len = strlen(s);
    char *buf = (char *)malloc(len + 1);
    if (buf != NULL) memcpy(buf, s, len + 1);
    return buf;
}
#else
;
#endif


/* Console I/O: Input(prompt), Print(msg) are already defined below as
 * pgy_input() and pgy_print(). See type_checker_builtins.c for semantic. */
