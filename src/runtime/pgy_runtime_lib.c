/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * pgy_runtime_lib.c — Non-inline runtime symbols for LLVM linking
 *
 * pgy_runtime.h uses static inline functions that are invisible to
 * the LLVM linker. This file provides real (extern) symbol definitions
 * with the EXACT names that LLVM IR references, so the linker can
 * resolve them.
 *
 * We do NOT include pgy_runtime.h here to avoid name collisions
 * between the static inline versions and our extern definitions.
 *
 * Only compiled when PGY_LLVM_ENABLED is defined.
 */

#ifdef PGY_LLVM_ENABLED

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "runtime/pgy_parallel.h"

/* =================================================================
 * Log functions
 * ================================================================= */

void pgy_log_int(int32_t v)    { printf("%d\n", v); }
void pgy_log_long(int64_t v)   { printf("%lld\n", (long long)v); }
void pgy_log_float(float v)    { printf("%f\n", v); }
void pgy_log_double(double v)  { printf("%lf\n", v); }
void pgy_log_bool(bool v)      { printf("%s\n", v ? "true" : "false"); }
void pgy_log_string(char *v)   { printf("%s\n", v ? v : "(null)"); }

char *pgy_int_to_string(int32_t v)
{
    char stack_buf[32];
    int len = snprintf(stack_buf, sizeof(stack_buf), "%d", v);
    if (len < 0) {
        char *fallback = (char *)malloc(2);
        if (fallback != NULL) {
            fallback[0] = '0';
            fallback[1] = '\0';
        }
        return fallback;
    }
    char *buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL) return NULL;
    memcpy(buf, stack_buf, (size_t)len + 1);
    return buf;
}

static struct timespec
pgy_runtime_deadline_after_ns(uint64_t timeout_ns)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (time_t)(timeout_ns / 1000000000ull);
    ts.tv_nsec += (long)(timeout_ns % 1000000000ull);
    if (ts.tv_nsec >= 1000000000l) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000l;
    }
    return ts;
}

/* =================================================================
 * Slot types (must match pgy_runtime.h layout)
 * ================================================================= */

typedef struct {
    int32_t value;
    bool    claimed;
} PgySlot_Int;

typedef struct {
    int64_t value;
    bool    claimed;
} PgySlot_Long;

typedef struct {
    float   value;
    bool    claimed;
} PgySlot_Float;

typedef struct {
    double  value;
    bool    claimed;
} PgySlot_Double;

typedef struct {
    bool    value;
    bool    claimed;
} PgySlot_Bool;

typedef struct {
    char   *value;
    bool    claimed;
} PgySlot_String;

/* =================================================================
 * Slot operations — Int
 * ================================================================= */

PgySlot_Int pgy_claim_Int(void)
{
    PgySlot_Int s;
    s.value = 0;
    s.claimed = true;
    return s;
}

void pgy_write_Int(PgySlot_Int *s, int32_t v)
{
    if (s != NULL)
        s->value = v;
}

int32_t pgy_read_Int(PgySlot_Int *s)
{
    if (s != NULL)
        return s->value;
    return 0;
}

void pgy_release_Int(PgySlot_Int *s)
{
    if (s != NULL) {
        s->value = 0;
        s->claimed = false;
    }
}

/* =================================================================
 * Slot operations — Long
 * ================================================================= */

PgySlot_Long pgy_claim_Long(void)
{
    PgySlot_Long s;
    s.value = 0;
    s.claimed = true;
    return s;
}

void pgy_write_Long(PgySlot_Long *s, int64_t v)
{
    if (s != NULL) s->value = v;
}

int64_t pgy_read_Long(PgySlot_Long *s)
{
    if (s != NULL) return s->value;
    return 0;
}

void pgy_release_Long(PgySlot_Long *s)
{
    if (s != NULL) { s->value = 0; s->claimed = false; }
}

/* =================================================================
 * Slot operations — Float
 * ================================================================= */

PgySlot_Float pgy_claim_Float(void)
{
    PgySlot_Float s;
    s.value = 0.0f;
    s.claimed = true;
    return s;
}

void pgy_write_Float(PgySlot_Float *s, float v)
{
    if (s != NULL) s->value = v;
}

float pgy_read_Float(PgySlot_Float *s)
{
    if (s != NULL) return s->value;
    return 0.0f;
}

void pgy_release_Float(PgySlot_Float *s)
{
    if (s != NULL) { s->value = 0.0f; s->claimed = false; }
}

/* =================================================================
 * Slot operations — Double
 * ================================================================= */

PgySlot_Double pgy_claim_Double(void)
{
    PgySlot_Double s;
    s.value = 0.0;
    s.claimed = true;
    return s;
}

void pgy_write_Double(PgySlot_Double *s, double v)
{
    if (s != NULL) s->value = v;
}

double pgy_read_Double(PgySlot_Double *s)
{
    if (s != NULL) return s->value;
    return 0.0;
}

void pgy_release_Double(PgySlot_Double *s)
{
    if (s != NULL) { s->value = 0.0; s->claimed = false; }
}

/* =================================================================
 * Slot operations — Bool
 * ================================================================= */

PgySlot_Bool pgy_claim_Bool(void)
{
    PgySlot_Bool s;
    s.value = false;
    s.claimed = true;
    return s;
}

void pgy_write_Bool(PgySlot_Bool *s, bool v)
{
    if (s != NULL) s->value = v;
}

bool pgy_read_Bool(PgySlot_Bool *s)
{
    if (s != NULL) return s->value;
    return false;
}

void pgy_release_Bool(PgySlot_Bool *s)
{
    if (s != NULL) { s->value = false; s->claimed = false; }
}

/* =================================================================
 * Slot operations — String
 * ================================================================= */

PgySlot_String pgy_claim_String(void)
{
    PgySlot_String s;
    s.value = NULL;
    s.claimed = true;
    return s;
}

void pgy_write_String(PgySlot_String *s, char *v)
{
    if (s != NULL)
        s->value = v;
}

char *pgy_read_String(PgySlot_String *s)
{
    if (s != NULL)
        return s->value;
    return NULL;
}

void pgy_release_String(PgySlot_String *s)
{
    if (s != NULL) {
        s->value = NULL;
        s->claimed = false;
    }
}

/* =================================================================
 * Secure slot operations — extern wrappers for LLVM linker
 * ================================================================= */

#define PGY_DEFINE_SECURE_SLOT_EXPORTS(Suffix, CType, ZeroExpr)                \
typedef struct {                                                               \
    CType    value;                                                            \
    bool     occupied;                                                         \
    uint64_t token;                                                            \
} PgySecureSlot_##Suffix;                                                      \
                                                                               \
typedef struct {                                                               \
    uint64_t id;                                                               \
    bool     can_write;                                                        \
    bool     can_read;                                                         \
} PgyToken_##Suffix;                                                           \
                                                                               \
PgySecureSlot_##Suffix pgy_claim_secure_##Suffix(PgyToken_##Suffix *out_token) \
{                                                                              \
    PgySecureSlot_##Suffix s;                                                  \
    s.value = (ZeroExpr);                                                      \
    s.occupied = true;                                                         \
    s.token = 0;                                                               \
    if (out_token != NULL) {                                                   \
        out_token->id = 0;                                                     \
        out_token->can_write = true;                                           \
        out_token->can_read = true;                                            \
    }                                                                          \
    return s;                                                                  \
}                                                                              \
                                                                               \
void pgy_secure_write_##Suffix(PgySecureSlot_##Suffix *s, CType v,             \
                               const PgyToken_##Suffix *t)                     \
{                                                                              \
    if (s != NULL && t != NULL && s->occupied                                  \
        && s->token == t->id && t->can_write)                                  \
        s->value = v;                                                          \
}                                                                              \
                                                                               \
CType pgy_secure_read_##Suffix(PgySecureSlot_##Suffix *s,                      \
                               const PgyToken_##Suffix *t)                     \
{                                                                              \
    if (s != NULL && t != NULL && s->occupied                                  \
        && s->token == t->id && t->can_read)                                   \
        return s->value;                                                       \
    return (ZeroExpr);                                                         \
}                                                                              \
                                                                               \
void pgy_secure_release_##Suffix(PgySecureSlot_##Suffix *s,                    \
                                 const PgyToken_##Suffix *t)                   \
{                                                                              \
    if (s != NULL && t != NULL && s->token == t->id) {                         \
        s->occupied = false;                                                   \
        s->token = 0;                                                          \
    }                                                                          \
}

PGY_DEFINE_SECURE_SLOT_EXPORTS(Int, int32_t, 0)
PGY_DEFINE_SECURE_SLOT_EXPORTS(Long, int64_t, 0)
PGY_DEFINE_SECURE_SLOT_EXPORTS(Float, float, 0.0f)
PGY_DEFINE_SECURE_SLOT_EXPORTS(Double, double, 0.0)
PGY_DEFINE_SECURE_SLOT_EXPORTS(Bool, bool, false)
PGY_DEFINE_SECURE_SLOT_EXPORTS(String, char *, NULL)

/* =================================================================
 * Device Slot operations — extern wrappers for LLVM linker
 * ================================================================= */

#define PGY_DEFINE_DEVICE_SLOT_EXPORTS(Suffix, CType, ZeroExpr)                 \
typedef struct {                                                                \
    CType value;                                                                \
    bool  claimed;                                                              \
} PgyDeviceSlot_##Suffix;                                                       \
                                                                                \
typedef struct {                                                                \
    PgyDeviceSlot_##Suffix *slot;                                               \
} PgyDeviceReadTaskArg_##Suffix;                                                \
                                                                                \
PgyDeviceSlot_##Suffix pgy_claim_device_##Suffix(void)                          \
{                                                                               \
    PgyDeviceSlot_##Suffix s;                                                   \
    s.value = (ZeroExpr);                                                       \
    s.claimed = true;                                                           \
    return s;                                                                   \
}                                                                               \
                                                                                \
void pgy_device_write_##Suffix(PgyDeviceSlot_##Suffix *s, CType v)              \
{                                                                               \
    if (s != NULL && s->claimed)                                                \
        s->value = v;                                                           \
}                                                                               \
                                                                                \
CType pgy_device_read_##Suffix(PgyDeviceSlot_##Suffix *s)                       \
{                                                                               \
    if (s != NULL && s->claimed)                                                \
        return s->value;                                                        \
    return (ZeroExpr);                                                          \
}                                                                               \
                                                                                \
void pgy_release_device_##Suffix(PgyDeviceSlot_##Suffix *s)                     \
{                                                                               \
    if (s != NULL) {                                                            \
        s->value = (ZeroExpr);                                                  \
        s->claimed = false;                                                     \
    }                                                                           \
}                                                                               \
                                                                                \
static void *pgy_device_read_task_##Suffix(void *raw)                           \
{                                                                               \
    PgyDeviceReadTaskArg_##Suffix *arg =                                        \
        (PgyDeviceReadTaskArg_##Suffix *)raw;                                   \
    CType *result = (CType *)malloc(sizeof(CType));                             \
    if (result == NULL) {                                                       \
        free(arg);                                                              \
        return NULL;                                                            \
    }                                                                           \
    *result = pgy_device_read_##Suffix(arg->slot);                              \
    free(arg);                                                                  \
    return result;                                                              \
}                                                                               \
                                                                                \
PgyTaskHandle pgy_submit_device_read_##Suffix(PgyDeviceSlot_##Suffix *s)        \
{                                                                               \
    PgyDeviceReadTaskArg_##Suffix *arg =                                        \
        (PgyDeviceReadTaskArg_##Suffix *)malloc(sizeof(PgyDeviceReadTaskArg_##Suffix)); \
    if (arg == NULL) {                                                          \
        PgyTaskHandle empty = {0};                                              \
        return empty;                                                           \
    }                                                                           \
    arg->slot = s;                                                              \
    return pgy_spawn(pgy_device_read_task_##Suffix, arg);                       \
}

PGY_DEFINE_DEVICE_SLOT_EXPORTS(Int, int32_t, 0)
PGY_DEFINE_DEVICE_SLOT_EXPORTS(Long, int64_t, 0)
PGY_DEFINE_DEVICE_SLOT_EXPORTS(Float, float, 0.0f)
PGY_DEFINE_DEVICE_SLOT_EXPORTS(Double, double, 0.0)
PGY_DEFINE_DEVICE_SLOT_EXPORTS(Bool, bool, false)
PGY_DEFINE_DEVICE_SLOT_EXPORTS(String, char *, NULL)

/* =================================================================
 * Array operations — extern wrappers for LLVM linker
 * ================================================================= */

#define PGY_DEFINE_ARRAY_EXPORTS(Suffix, CType)                                  \
typedef struct {                                                                 \
    CType  *data;                                                                \
    size_t  length;                                                              \
    size_t  capacity;                                                            \
    void   *allocator;                                                           \
} PgyArray_##Suffix;                                                             \
                                                                                 \
PgyArray_##Suffix pgy_array_new_##Suffix(size_t capacity)                        \
{                                                                                \
    PgyArray_##Suffix arr;                                                       \
    arr.length = 0;                                                              \
    arr.capacity = capacity;                                                     \
    arr.allocator = NULL;                                                        \
    arr.data = capacity > 0                                                      \
        ? (CType *)malloc(sizeof(CType) * capacity)                              \
        : NULL;                                                                  \
    return arr;                                                                  \
}                                                                                \
                                                                                 \
void pgy_array_push_##Suffix(PgyArray_##Suffix *arr, CType value)                \
{                                                                                \
    if (arr == NULL)                                                             \
        return;                                                                  \
    if (arr->length == arr->capacity) {                                          \
        size_t next = arr->capacity == 0 ? 4 : arr->capacity * 2;                \
        CType *next_data = arr->data == NULL                                     \
            ? (CType *)malloc(sizeof(CType) * next)                              \
            : (CType *)realloc(arr->data, sizeof(CType) * next);                 \
        if (next_data == NULL)                                                   \
            return;                                                              \
        arr->data = next_data;                                                   \
        arr->capacity = next;                                                    \
    }                                                                            \
    arr->data[arr->length++] = value;                                            \
}

PGY_DEFINE_ARRAY_EXPORTS(Int, int32_t)
PGY_DEFINE_ARRAY_EXPORTS(Long, int64_t)
PGY_DEFINE_ARRAY_EXPORTS(Float, float)
PGY_DEFINE_ARRAY_EXPORTS(Double, double)
PGY_DEFINE_ARRAY_EXPORTS(Bool, bool)
PGY_DEFINE_ARRAY_EXPORTS(String, char *)

static char *
pgy_runtime_lib_strdup(const char *src)
{
    if (src == NULL)
        src = "";

    size_t len = strlen(src);
    char *copy = (char *)malloc(len + 1);
    if (copy == NULL)
        return NULL;

    memcpy(copy, src, len + 1);
    return copy;
}

/* =================================================================
 * File I/O and string helpers needed by LLVM backend
 * ================================================================= */

#define PGY_MAX_OPEN_FILES 256

static FILE *pgy_runtime_ftable[PGY_MAX_OPEN_FILES];
static int   pgy_runtime_ftable_next = 3;

static void
pgy_runtime_io_init(void)
{
    pgy_runtime_ftable[0] = stdin;
    pgy_runtime_ftable[1] = stdout;
    pgy_runtime_ftable[2] = stderr;
}

int32_t pgy_file_open(const char *path, const char *mode)
{
    if (pgy_runtime_ftable[0] == NULL)
        pgy_runtime_io_init();

    FILE *fp = fopen(path, mode);
    if (fp == NULL)
        return -1;
    if (pgy_runtime_ftable_next >= PGY_MAX_OPEN_FILES) {
        fclose(fp);
        return -1;
    }

    int fd = pgy_runtime_ftable_next++;
    pgy_runtime_ftable[fd] = fp;
    return (int32_t)fd;
}

char *pgy_file_read(int32_t fd)
{
    char tmp[4096];

    tmp[0] = '\0';
    if (fd < 0 || fd >= PGY_MAX_OPEN_FILES || pgy_runtime_ftable[fd] == NULL)
        return pgy_runtime_lib_strdup("");
    if (fgets(tmp, sizeof(tmp), pgy_runtime_ftable[fd]) == NULL)
        return pgy_runtime_lib_strdup("");

    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '\n')
        tmp[len - 1] = '\0';
    return pgy_runtime_lib_strdup(tmp);
}

void pgy_file_write(int32_t fd, const char *data)
{
    if (fd < 0 || fd >= PGY_MAX_OPEN_FILES || pgy_runtime_ftable[fd] == NULL)
        return;
    if (data != NULL)
        fwrite(data, 1, strlen(data), pgy_runtime_ftable[fd]);
}

void pgy_file_close(int32_t fd)
{
    if (fd < 3 || fd >= PGY_MAX_OPEN_FILES || pgy_runtime_ftable[fd] == NULL)
        return;
    fclose(pgy_runtime_ftable[fd]);
    pgy_runtime_ftable[fd] = NULL;
}

char *pgy_read_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
        return pgy_runtime_lib_strdup("");

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (len < 0) {
        fclose(fp);
        return pgy_runtime_lib_strdup("");
    }

    char *buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL) {
        fclose(fp);
        return pgy_runtime_lib_strdup("");
    }

    size_t read_len = fread(buf, 1, (size_t)len, fp);
    buf[read_len] = '\0';
    fclose(fp);
    return buf;
}

void pgy_write_file(const char *path, const char *data)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL)
        return;
    if (data != NULL)
        fwrite(data, 1, strlen(data), fp);
    fclose(fp);
}

char *pgy_input(const char *prompt)
{
    static char input_buf[4096];

    if (prompt != NULL && prompt[0] != '\0')
        printf("%s", prompt);
    fflush(stdout);

    input_buf[0] = '\0';
    if (fgets(input_buf, sizeof(input_buf), stdin) == NULL)
        return input_buf;

    size_t len = strlen(input_buf);
    if (len > 0 && input_buf[len - 1] == '\n')
        input_buf[len - 1] = '\0';
    return input_buf;
}

bool StringContains(const char *haystack, const char *needle)
{
    if (haystack == NULL || needle == NULL)
        return false;
    return strstr(haystack, needle) != NULL;
}

char *Substring(const char *s, int32_t start, int32_t len)
{
    if (s == NULL)
        return pgy_runtime_lib_strdup("");

    int32_t slen = (int32_t)strlen(s);
    if (start < 0 || start >= slen || len <= 0)
        return pgy_runtime_lib_strdup("");
    if (start + len > slen)
        len = slen - start;

    char *buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL)
        return pgy_runtime_lib_strdup("");
    memcpy(buf, s + start, (size_t)len);
    buf[len] = '\0';
    return buf;
}

char *StringReplace(const char *s, const char *old_str, const char *new_str)
{
    if (s == NULL || old_str == NULL || new_str == NULL)
        return pgy_runtime_lib_strdup(s != NULL ? s : "");

    size_t old_len = strlen(old_str);
    size_t new_len = strlen(new_str);
    if (old_len == 0)
        return pgy_runtime_lib_strdup(s);

    int count = 0;
    const char *p = s;
    while ((p = strstr(p, old_str)) != NULL) {
        count++;
        p += old_len;
    }

    size_t result_len = strlen(s) + (size_t)count * (new_len - old_len);
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
    char *buf = (char *)malloc(la + lb + 1);
    if (buf == NULL)
        return pgy_runtime_lib_strdup("");
    memcpy(buf, a, la);
    memcpy(buf + la, b, lb + 1);
    return buf;
}

/* -----------------------------------------------------------------
 * StringSplit / StringJoin / ToInt / ToFloat / Math
 * ----------------------------------------------------------------- */

/* StringSplit(str, delim) → Array<String> (caller-allocated PgyArray_String) */
PgyArray_String StringSplit(const char *s, const char *delim)
{
    PgyArray_String result = pgy_array_new_String(8);
    if (s == NULL || delim == NULL || *delim == '\0') {
        if (s != NULL)
            pgy_array_push_String(&result, pgy_runtime_lib_strdup(s));
        return result;
    }
    size_t dlen = strlen(delim);
    const char *p = s;
    for (;;) {
        const char *found = strstr(p, delim);
        if (found == NULL) {
            pgy_array_push_String(&result, pgy_runtime_lib_strdup(p));
            break;
        }
        size_t seg = (size_t)(found - p);
        char *part = (char *)malloc(seg + 1);
        if (part != NULL) { memcpy(part, p, seg); part[seg] = '\0'; }
        pgy_array_push_String(&result, part != NULL ? part : pgy_runtime_lib_strdup(""));
        p = found + dlen;
    }
    return result;
}

/* StringJoin(arr, sep) → String */
char *StringJoin(PgyArray_String *arr, const char *sep)
{
    if (arr == NULL || arr->length == 0)
        return pgy_runtime_lib_strdup("");
    size_t slen = (sep != NULL) ? strlen(sep) : 0;
    size_t total = 0;
    for (size_t i = 0; i < arr->length; i++) {
        if (arr->data[i] != NULL) total += strlen(arr->data[i]);
        if (i > 0) total += slen;
    }
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

/* =================================================================
 * Channel — Int (thread-safe with mutex + condvar)
 * ================================================================= */

#include <pthread.h>

/* PgyOption_Bool — needed for try_send_status / send_timeout_status.
 * Must match PGY_OPTION_DEFINE(Bool, bool) in pgy_runtime.h. */
typedef struct { int tag; bool value; } PgyOption_Bool;
static inline PgyOption_Bool Some_Bool(bool v) { return (PgyOption_Bool){ 1, v }; }
static inline PgyOption_Bool None_Bool(void)   { return (PgyOption_Bool){ 0, false }; }

typedef struct {
    int32_t        *buffer;
    size_t          capacity;
    size_t          head;
    size_t          tail;
    size_t          count;
    bool            closed;
    pthread_mutex_t mutex;
    pthread_cond_t  cond_not_full;
    pthread_cond_t  cond_not_empty;
} PgyChannel_Int_RT;

void pgy_channel_init_Int(PgyChannel_Int_RT *ch, size_t cap)
{
    if (ch == NULL) return;
    ch->buffer   = (int32_t *)calloc(cap, sizeof(int32_t));
    ch->capacity = cap;
    ch->head     = 0;
    ch->tail     = 0;
    ch->count    = 0;
    ch->closed   = false;
    pthread_mutex_init(&ch->mutex, NULL);
    pthread_cond_init(&ch->cond_not_full, NULL);
    pthread_cond_init(&ch->cond_not_empty, NULL);
}

void pgy_channel_destroy_Int(PgyChannel_Int_RT *ch)
{
    if (ch == NULL) return;
    pthread_mutex_destroy(&ch->mutex);
    pthread_cond_destroy(&ch->cond_not_full);
    pthread_cond_destroy(&ch->cond_not_empty);
    free(ch->buffer);
    ch->buffer = NULL;
}

void pgy_channel_close_Int(PgyChannel_Int_RT *ch)
{
    if (ch == NULL) return;
    pthread_mutex_lock(&ch->mutex);
    ch->closed = true;
    pthread_cond_broadcast(&ch->cond_not_full);
    pthread_cond_broadcast(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
}

bool pgy_channel_send_Int(PgyChannel_Int_RT *ch, int32_t v)
{
    if (ch == NULL) return false;
    pthread_mutex_lock(&ch->mutex);
    while (ch->count >= ch->capacity && !ch->closed)
        pthread_cond_wait(&ch->cond_not_full, &ch->mutex);
    if (ch->closed) {
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    ch->buffer[ch->tail] = v;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

bool pgy_channel_try_send_Int(PgyChannel_Int_RT *ch, int32_t v)
{
    if (ch == NULL) return false;
    pthread_mutex_lock(&ch->mutex);
    if (ch->closed || ch->count >= ch->capacity) {
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    ch->buffer[ch->tail] = v;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

bool pgy_channel_send_timeout_Int(PgyChannel_Int_RT *ch, int32_t v,
                                  uint64_t timeout_ns)
{
    if (ch == NULL) return false;
    struct timespec deadline = pgy_runtime_deadline_after_ns(timeout_ns);
    pthread_mutex_lock(&ch->mutex);
    while (ch->count >= ch->capacity && !ch->closed) {
        if (pthread_cond_timedwait(&ch->cond_not_full, &ch->mutex, &deadline)
            == ETIMEDOUT && ch->count >= ch->capacity && !ch->closed) {
            pthread_mutex_unlock(&ch->mutex);
            return false;
        }
    }
    if (ch->closed) {
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    ch->buffer[ch->tail] = v;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

bool pgy_channel_recv_Int(PgyChannel_Int_RT *ch, int32_t *out)
{
    if (ch == NULL || out == NULL) return false;
    pthread_mutex_lock(&ch->mutex);
    while (ch->count == 0 && !ch->closed)
        pthread_cond_wait(&ch->cond_not_empty, &ch->mutex);
    if (ch->count == 0 && ch->closed) {
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    *out = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    pthread_cond_signal(&ch->cond_not_full);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

bool pgy_channel_ready_Int(PgyChannel_Int_RT *ch)
{
    if (ch == NULL) return false;
    pthread_mutex_lock(&ch->mutex);
    bool ready = ch->count > 0;
    pthread_mutex_unlock(&ch->mutex);
    return ready;
}

int32_t pgy_channel_length_Int(PgyChannel_Int_RT *ch)
{
    if (ch == NULL) return 0;
    pthread_mutex_lock(&ch->mutex);
    int32_t len = (int32_t)ch->count;
    pthread_mutex_unlock(&ch->mutex);
    return len;
}

int32_t pgy_channel_capacity_Int(PgyChannel_Int_RT *ch)
{
    if (ch == NULL) return 0;
    pthread_mutex_lock(&ch->mutex);
    int32_t cap = (int32_t)ch->capacity;
    pthread_mutex_unlock(&ch->mutex);
    return cap;
}

bool pgy_channel_full_Int(PgyChannel_Int_RT *ch)
{
    if (ch == NULL) return false;
    pthread_mutex_lock(&ch->mutex);
    bool full = ch->count >= ch->capacity;
    pthread_mutex_unlock(&ch->mutex);
    return full;
}

int32_t pgy_channel_space_Int(PgyChannel_Int_RT *ch)
{
    if (ch == NULL) return 0;
    pthread_mutex_lock(&ch->mutex);
    int32_t space = (int32_t)(ch->capacity - ch->count);
    pthread_mutex_unlock(&ch->mutex);
    return space;
}

bool pgy_channel_closed_Int(PgyChannel_Int_RT *ch)
{
    if (ch == NULL) return true;
    pthread_mutex_lock(&ch->mutex);
    bool closed = ch->closed;
    pthread_mutex_unlock(&ch->mutex);
    return closed;
}

bool pgy_channel_try_recv_Int(PgyChannel_Int_RT *ch, int32_t *out)
{
    if (ch == NULL || out == NULL) return false;
    pthread_mutex_lock(&ch->mutex);
    if (ch->count == 0) {
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    *out = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    pthread_cond_signal(&ch->cond_not_full);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

bool pgy_channel_recv_timeout_Int(PgyChannel_Int_RT *ch, int32_t *out,
                                  uint64_t timeout_ns)
{
    if (ch == NULL || out == NULL) return false;
    struct timespec deadline = pgy_runtime_deadline_after_ns(timeout_ns);
    pthread_mutex_lock(&ch->mutex);
    while (ch->count == 0 && !ch->closed) {
        if (pthread_cond_timedwait(&ch->cond_not_empty, &ch->mutex, &deadline)
            == ETIMEDOUT && ch->count == 0 && !ch->closed) {
            pthread_mutex_unlock(&ch->mutex);
            return false;
        }
    }
    if (ch->count == 0 && ch->closed) {
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    *out = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    pthread_cond_signal(&ch->cond_not_full);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

int32_t pgy_channel_recv_val_Int(PgyChannel_Int_RT *ch)
{
    int32_t out = 0;
    pgy_channel_recv_Int(ch, &out);
    return out;
}

typedef struct {
    char **buffer;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    bool closed;
    pthread_mutex_t mutex;
    pthread_cond_t cond_not_full;
    pthread_cond_t cond_not_empty;
} PgyChannel_String_RT;

void pgy_channel_init_String(PgyChannel_String_RT *ch, size_t cap)
{
    if (ch == NULL) return;
    ch->buffer = (char **)calloc(cap, sizeof(char *));
    ch->capacity = cap;
    ch->head = 0;
    ch->tail = 0;
    ch->count = 0;
    ch->closed = false;
    pthread_mutex_init(&ch->mutex, NULL);
    pthread_cond_init(&ch->cond_not_full, NULL);
    pthread_cond_init(&ch->cond_not_empty, NULL);
}

void pgy_channel_destroy_String(PgyChannel_String_RT *ch)
{
    if (ch == NULL) return;
    pthread_mutex_destroy(&ch->mutex);
    pthread_cond_destroy(&ch->cond_not_full);
    pthread_cond_destroy(&ch->cond_not_empty);
    free(ch->buffer);
    ch->buffer = NULL;
}

void pgy_channel_close_String(PgyChannel_String_RT *ch)
{
    if (ch == NULL) return;
    pthread_mutex_lock(&ch->mutex);
    ch->closed = true;
    pthread_cond_broadcast(&ch->cond_not_full);
    pthread_cond_broadcast(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
}

bool pgy_channel_send_String(PgyChannel_String_RT *ch, char *v)
{
    if (ch == NULL) return false;
    pthread_mutex_lock(&ch->mutex);
    while (ch->count >= ch->capacity && !ch->closed)
        pthread_cond_wait(&ch->cond_not_full, &ch->mutex);
    if (ch->closed) {
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    ch->buffer[ch->tail] = v;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

bool pgy_channel_try_send_String(PgyChannel_String_RT *ch, char *v)
{
    if (ch == NULL) return false;
    pthread_mutex_lock(&ch->mutex);
    if (ch->closed || ch->count >= ch->capacity) {
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    ch->buffer[ch->tail] = v;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

bool pgy_channel_send_timeout_String(PgyChannel_String_RT *ch, char *v,
                                     uint64_t timeout_ns)
{
    if (ch == NULL) return false;
    struct timespec deadline = pgy_runtime_deadline_after_ns(timeout_ns);
    pthread_mutex_lock(&ch->mutex);
    while (ch->count >= ch->capacity && !ch->closed) {
        if (pthread_cond_timedwait(&ch->cond_not_full, &ch->mutex, &deadline)
            == ETIMEDOUT && ch->count >= ch->capacity && !ch->closed) {
            pthread_mutex_unlock(&ch->mutex);
            return false;
        }
    }
    if (ch->closed) {
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    ch->buffer[ch->tail] = v;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

bool pgy_channel_recv_String(PgyChannel_String_RT *ch, char **out)
{
    if (ch == NULL || out == NULL) return false;
    pthread_mutex_lock(&ch->mutex);
    while (ch->count == 0 && !ch->closed)
        pthread_cond_wait(&ch->cond_not_empty, &ch->mutex);
    if (ch->count == 0 && ch->closed) {
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    *out = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    pthread_cond_signal(&ch->cond_not_full);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

bool pgy_channel_recv_timeout_String(PgyChannel_String_RT *ch, char **out,
                                     uint64_t timeout_ns)
{
    if (ch == NULL || out == NULL) return false;
    struct timespec deadline = pgy_runtime_deadline_after_ns(timeout_ns);
    pthread_mutex_lock(&ch->mutex);
    while (ch->count == 0 && !ch->closed) {
        if (pthread_cond_timedwait(&ch->cond_not_empty, &ch->mutex, &deadline)
            == ETIMEDOUT && ch->count == 0 && !ch->closed) {
            pthread_mutex_unlock(&ch->mutex);
            return false;
        }
    }
    if (ch->count == 0 && ch->closed) {
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    *out = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    pthread_cond_signal(&ch->cond_not_full);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

bool pgy_channel_try_recv_String(PgyChannel_String_RT *ch, char **out)
{
    if (ch == NULL || out == NULL) return false;
    pthread_mutex_lock(&ch->mutex);
    if (ch->count == 0) {
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    *out = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    pthread_cond_signal(&ch->cond_not_full);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

bool pgy_channel_ready_String(PgyChannel_String_RT *ch)
{
    if (ch == NULL) return false;
    pthread_mutex_lock(&ch->mutex);
    bool ready = ch->count > 0;
    pthread_mutex_unlock(&ch->mutex);
    return ready;
}

int32_t pgy_channel_length_String(PgyChannel_String_RT *ch)
{
    if (ch == NULL) return 0;
    pthread_mutex_lock(&ch->mutex);
    int32_t len = (int32_t)ch->count;
    pthread_mutex_unlock(&ch->mutex);
    return len;
}

int32_t pgy_channel_capacity_String(PgyChannel_String_RT *ch)
{
    if (ch == NULL) return 0;
    pthread_mutex_lock(&ch->mutex);
    int32_t cap = (int32_t)ch->capacity;
    pthread_mutex_unlock(&ch->mutex);
    return cap;
}

bool pgy_channel_full_String(PgyChannel_String_RT *ch)
{
    if (ch == NULL) return false;
    pthread_mutex_lock(&ch->mutex);
    bool full = ch->count >= ch->capacity;
    pthread_mutex_unlock(&ch->mutex);
    return full;
}

int32_t pgy_channel_space_String(PgyChannel_String_RT *ch)
{
    if (ch == NULL) return 0;
    pthread_mutex_lock(&ch->mutex);
    int32_t space = (int32_t)(ch->capacity - ch->count);
    pthread_mutex_unlock(&ch->mutex);
    return space;
}

bool pgy_channel_closed_String(PgyChannel_String_RT *ch)
{
    if (ch == NULL) return true;
    pthread_mutex_lock(&ch->mutex);
    bool closed = ch->closed;
    pthread_mutex_unlock(&ch->mutex);
    return closed;
}

char *pgy_channel_recv_val_String(PgyChannel_String_RT *ch)
{
    char *out = NULL;
    pgy_channel_recv_String(ch, &out);
    return out;
}

/* =================================================================
 * QubitSlot runtime — N-qubit entanglement pool model
 *
 * Matches pgy_runtime.h's pool_id / PgyEntanglementPool design.
 * Supports GHZ states via pool merge on Entangle().
 * ================================================================= */

#define PGY_QUBIT_RT_MAX 64

typedef struct {
    int32_t state;       /* 0=|0>, 1=|1>, 2=superposition, -1=released */
    int32_t pool_id;     /* entanglement pool id, -1 if none */
    bool    measured;
} PgyQubit_RT;

typedef struct {
    int32_t members[PGY_QUBIT_RT_MAX];
    int32_t count;
    bool    active;
} PgyEntanglementPool_RT;

static PgyQubit_RT              pgy_qubits_rt[PGY_QUBIT_RT_MAX];
static int32_t                  pgy_qubit_next_rt = 0;
static bool                     pgy_qubit_rng_init_rt = false;

static PgyEntanglementPool_RT   pgy_qubit_pools_rt[PGY_QUBIT_RT_MAX];
static int32_t                  pgy_qubit_pool_next_rt = 0;

/* --- Pool helpers --- */

static int32_t
rt_alloc_pool(void)
{
    if (pgy_qubit_pool_next_rt >= PGY_QUBIT_RT_MAX) return -1;
    int32_t id = pgy_qubit_pool_next_rt++;
    pgy_qubit_pools_rt[id].count  = 0;
    pgy_qubit_pools_rt[id].active = true;
    return id;
}

static void
rt_pool_add(int32_t pool_id, int32_t qubit_id)
{
    if (pool_id < 0 || pool_id >= PGY_QUBIT_RT_MAX) return;
    PgyEntanglementPool_RT *pool = &pgy_qubit_pools_rt[pool_id];
    if (pool->count >= PGY_QUBIT_RT_MAX) return;
    for (int32_t i = 0; i < pool->count; i++)
        if (pool->members[i] == qubit_id) return;
    pool->members[pool->count++] = qubit_id;
    pgy_qubits_rt[qubit_id].pool_id = pool_id;
}

static void
rt_pool_remove(int32_t pool_id, int32_t qubit_id)
{
    if (pool_id < 0 || pool_id >= PGY_QUBIT_RT_MAX) return;
    PgyEntanglementPool_RT *pool = &pgy_qubit_pools_rt[pool_id];
    for (int32_t i = 0; i < pool->count; i++) {
        if (pool->members[i] == qubit_id) {
            pool->members[i] = pool->members[pool->count - 1];
            pool->count--;
            return;
        }
    }
}

static void
rt_pool_merge(int32_t dst_pool, int32_t src_pool)
{
    if (dst_pool == src_pool) return;
    if (dst_pool < 0 || src_pool < 0) return;
    PgyEntanglementPool_RT *src = &pgy_qubit_pools_rt[src_pool];
    for (int32_t i = 0; i < src->count; i++) {
        int32_t qid = src->members[i];
        rt_pool_add(dst_pool, qid);
    }
    src->count  = 0;
    src->active = false;
}

/* --- Qubit operations --- */

int32_t ClaimQubit(void)
{
    if (!pgy_qubit_rng_init_rt) {
        srand((unsigned)time(NULL));
        pgy_qubit_rng_init_rt = true;
    }
    if (pgy_qubit_next_rt >= PGY_QUBIT_RT_MAX)
        return -1;

    int32_t id = pgy_qubit_next_rt++;
    pgy_qubits_rt[id].state    = 2;
    pgy_qubits_rt[id].pool_id  = -1;
    pgy_qubits_rt[id].measured = false;
    return id;
}

int32_t Measure(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_RT_MAX)
        return -1;

    PgyQubit_RT *q = &pgy_qubits_rt[id];
    if (q->measured)
        return q->state;

    if (q->state == 2)
        q->state = rand() % 2;
    q->measured = true;

    /* Propagate collapse to entire entanglement pool */
    if (q->pool_id >= 0) {
        PgyEntanglementPool_RT *pool = &pgy_qubit_pools_rt[q->pool_id];
        for (int32_t i = 0; i < pool->count; i++) {
            int32_t mid = pool->members[i];
            if (mid != id && !pgy_qubits_rt[mid].measured) {
                pgy_qubits_rt[mid].state    = q->state;
                pgy_qubits_rt[mid].measured = true;
            }
        }
    }

    return q->state;
}

void Entangle(int32_t a, int32_t b)
{
    if (a < 0 || a >= PGY_QUBIT_RT_MAX || b < 0 || b >= PGY_QUBIT_RT_MAX)
        return;

    int32_t pa = pgy_qubits_rt[a].pool_id;
    int32_t pb = pgy_qubits_rt[b].pool_id;

    if (pa >= 0 && pb >= 0) {
        if (pa != pb)
            rt_pool_merge(pa, pb);
    } else if (pa >= 0) {
        rt_pool_add(pa, b);
    } else if (pb >= 0) {
        rt_pool_add(pb, a);
    } else {
        int32_t new_pool = rt_alloc_pool();
        if (new_pool >= 0) {
            rt_pool_add(new_pool, a);
            rt_pool_add(new_pool, b);
        }
    }
}

void H(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_RT_MAX) return;
    pgy_qubits_rt[id].state    = 2;
    pgy_qubits_rt[id].measured = false;
}

bool IntoClassical(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_RT_MAX) return false;
    return pgy_qubits_rt[id].state == 1;
}

int32_t QubitState(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_RT_MAX)
        return -1;
    return pgy_qubits_rt[id].state;
}

bool IsCollapsed(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_RT_MAX)
        return true;
    return pgy_qubits_rt[id].measured;
}

void ReleaseQubit(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_RT_MAX)
        return;
    if (pgy_qubits_rt[id].pool_id >= 0)
        rt_pool_remove(pgy_qubits_rt[id].pool_id, id);
    pgy_qubits_rt[id].state    = -1;
    pgy_qubits_rt[id].pool_id  = -1;
    pgy_qubits_rt[id].measured = true;
}

/* =================================================================
 * Thread pool runtime (real pthread-based concurrency)
 *
 * These are non-inline exports of the pgy_parallel.h functions.
 * The LLVM backend links against these symbols.
 * ================================================================= */

#include "runtime/pgy_parallel.h"

/* Force non-inline symbol exports for the linker */
void pgy_pool_init_export(size_t n)    { pgy_pool_init(n); }
void pgy_pool_shutdown_export(void)    { pgy_pool_shutdown(); }

PgyTaskHandle pgy_spawn_export(void *(*fn)(void *), void *arg)
{
    return pgy_spawn(fn, arg);
}

PgyTaskHandle pgy_async_spawn_export(void *(*fn)(void *), void *arg)
{
    return pgy_async_spawn(fn, arg);
}

void pgy_async_detach_export(PgyTaskHandle h)
{
    pgy_async_detach(h);
}

void *pgy_await_export(PgyTaskHandle h)
{
    return pgy_await(h);
}

PgyTaskHandle pgy_spawn_blocking_export(void *(*fn)(void *), void *arg)
{
    return pgy_spawn_blocking(fn, arg);
}

bool pgy_task_cancel_export(PgyTaskHandle h)
{
    return pgy_task_cancel(h);
}

bool pgy_task_is_cancelled_export(void)
{
    return pgy_task_is_cancelled();
}

#endif /* PGY_LLVM_ENABLED */
