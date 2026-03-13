/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * pgy_runtime.h — C runtime support library for transpiled Pergyra code.
 *
 * Every .c file produced by the transpiler starts with:
 *   #include "pgy_runtime.h"
 *
 * Slot model:
 *   A slot is a typed wrapper around a value plus an occupancy flag.
 *   SecureSlots add a 64-bit token for access control.
 *   All slot operations are inline to keep overhead near zero.
 */

#ifndef PGY_RUNTIME_H
#define PGY_RUNTIME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -----------------------------------------------------------------
 * Panic — unrecoverable error
 * ----------------------------------------------------------------- */

#define PGY_PANIC(msg) \
    do { \
        fprintf(stderr, "[PGY PANIC] %s:%d — %s\n", \
                __FILE__, __LINE__, (msg)); \
        abort(); \
    } while (0)

/* -----------------------------------------------------------------
 * Slot macros
 *
 * PGY_SLOT_DEFINE(SuffixName, CType)
 *   Defines:
 *     PgySlot_SuffixName         — struct type
 *     pgy_claim_SuffixName()     — allocate + mark occupied
 *     pgy_write_SuffixName(s, v) — write value
 *     pgy_read_SuffixName(s)     — read value
 *     pgy_release_SuffixName(s)  — mark unoccupied
 *
 * PGY_SECURE_SLOT_DEFINE(SuffixName, CType)
 *   Defines the same but with a uint64_t token field and
 *   token-checking variants.
 * ----------------------------------------------------------------- */

#define PGY_SLOT_DEFINE(SuffixName, CType) \
\
typedef struct { \
    CType   value; \
    bool    occupied; \
} PgySlot_##SuffixName; \
\
static inline PgySlot_##SuffixName \
pgy_claim_##SuffixName(void) \
{ \
    PgySlot_##SuffixName s; \
    memset(&s, 0, sizeof(s)); \
    s.occupied = true; \
    return s; \
} \
\
static inline void \
pgy_write_##SuffixName(PgySlot_##SuffixName *s, CType v) \
{ \
    if (!s->occupied) PGY_PANIC("write to released slot"); \
    s->value = v; \
} \
\
static inline CType \
pgy_read_##SuffixName(PgySlot_##SuffixName *s) \
{ \
    if (!s->occupied) PGY_PANIC("read from released slot"); \
    return s->value; \
} \
\
static inline void \
pgy_release_##SuffixName(PgySlot_##SuffixName *s) \
{ \
    if (!s->occupied) PGY_PANIC("double release of slot"); \
    s->occupied = false; \
}

/* -----------------------------------------------------------------
 * Secure slot — adds token-based access control
 * ----------------------------------------------------------------- */

#define PGY_SECURE_SLOT_DEFINE(SuffixName, CType) \
\
typedef struct { \
    CType    value; \
    bool     occupied; \
    uint64_t token; \
} PgySecureSlot_##SuffixName; \
\
typedef struct { \
    uint64_t id;      /* must match the slot's token field */ \
    bool     can_write; \
    bool     can_read; \
} PgyToken_##SuffixName; \
\
static inline void \
pgy_make_token_##SuffixName(PgySecureSlot_##SuffixName *s, \
                             PgyToken_##SuffixName *t) \
{ \
    /* Simple non-cryptographic token for prototype purposes */ \
    uint64_t id = (uint64_t)(uintptr_t)s ^ 0xDEADBEEFCAFEBABEULL; \
    s->token    = id; \
    t->id       = id; \
    t->can_write = true; \
    t->can_read  = true; \
} \
\
static inline PgySecureSlot_##SuffixName \
pgy_claim_secure_##SuffixName(PgyToken_##SuffixName *out_token) \
{ \
    PgySecureSlot_##SuffixName s; \
    memset(&s, 0, sizeof(s)); \
    s.occupied = true; \
    pgy_make_token_##SuffixName(&s, out_token); \
    return s; \
} \
\
static inline void \
pgy_secure_write_##SuffixName(PgySecureSlot_##SuffixName *s, \
                               CType v, \
                               const PgyToken_##SuffixName *t) \
{ \
    if (!s->occupied)   PGY_PANIC("write to released secure slot"); \
    if (s->token != t->id) PGY_PANIC("invalid token on write"); \
    if (!t->can_write)  PGY_PANIC("token does not allow write"); \
    s->value = v; \
} \
\
static inline CType \
pgy_secure_read_##SuffixName(PgySecureSlot_##SuffixName *s, \
                              const PgyToken_##SuffixName *t) \
{ \
    if (!s->occupied)   PGY_PANIC("read from released secure slot"); \
    if (s->token != t->id) PGY_PANIC("invalid token on read"); \
    if (!t->can_read)   PGY_PANIC("token does not allow read"); \
    return s->value; \
} \
\
static inline void \
pgy_secure_release_##SuffixName(PgySecureSlot_##SuffixName *s, \
                                 const PgyToken_##SuffixName *t) \
{ \
    if (!s->occupied)   PGY_PANIC("double release of secure slot"); \
    if (s->token != t->id) PGY_PANIC("invalid token on release"); \
    s->occupied = false; \
    s->token    = 0; \
}

/* -----------------------------------------------------------------
 * Instantiate built-in slot types
 * ----------------------------------------------------------------- */

PGY_SLOT_DEFINE(Int,    int32_t)
PGY_SLOT_DEFINE(Long,   int64_t)
PGY_SLOT_DEFINE(Float,  float)
PGY_SLOT_DEFINE(Double, double)
PGY_SLOT_DEFINE(Bool,   bool)
PGY_SLOT_DEFINE(String, char*)

PGY_SECURE_SLOT_DEFINE(Int,    int32_t)
PGY_SECURE_SLOT_DEFINE(Long,   int64_t)
PGY_SECURE_SLOT_DEFINE(Float,  float)
PGY_SECURE_SLOT_DEFINE(Double, double)
PGY_SECURE_SLOT_DEFINE(Bool,   bool)
PGY_SECURE_SLOT_DEFINE(String, char*)

/* -----------------------------------------------------------------
 * Log — maps Pergyra Log() to printf
 * ----------------------------------------------------------------- */

static inline void pgy_log_int(int32_t v)    { printf("%d\n",  v); }
static inline void pgy_log_long(int64_t v)   { printf("%lld\n",(long long)v); }
static inline void pgy_log_float(float v)    { printf("%f\n",  v); }
static inline void pgy_log_double(double v)  { printf("%lf\n", v); }
static inline void pgy_log_bool(bool v)      { printf("%s\n",  v ? "true" : "false"); }
static inline void pgy_log_string(char *v)   { printf("%s\n",  v ? v : "(null)"); }

/* Generic macro — resolves by type via _Generic (C11) */
#define pgy_log(x) _Generic((x), \
    int32_t:  pgy_log_int,    \
    int64_t:  pgy_log_long,   \
    float:    pgy_log_float,  \
    double:   pgy_log_double, \
    bool:     pgy_log_bool,   \
    char*:    pgy_log_string  \
)(x)

/* -----------------------------------------------------------------
 * Parallel support
 *
 * If compiled with -fopenmp the Parallel block maps to
 * OpenMP sections. Without OpenMP the tasks run sequentially.
 * ----------------------------------------------------------------- */

#ifdef _OPENMP
#  include <omp.h>
#  define PGY_PARALLEL_BEGIN \
       _Pragma("omp parallel sections")  {
#  define PGY_PARALLEL_TASK  _Pragma("omp section")
#  define PGY_PARALLEL_END   }
#else
#  define PGY_PARALLEL_BEGIN {
#  define PGY_PARALLEL_TASK  /* sequential */
#  define PGY_PARALLEL_END   }
#endif

#endif /* PGY_RUNTIME_H */
