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

#ifndef PGY_INTENT_OBSERVABILITY_ENABLED
#define PGY_INTENT_OBSERVABILITY_ENABLED 1
#endif

#ifndef PGY_RUNTIME_MAX_FILE_BYTES
#define PGY_RUNTIME_MAX_FILE_BYTES (64u * 1024u * 1024u)
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include <pthread.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif
#include "runtime/pgy_parallel.h"

static char *pgy_runtime_lib_strdup(const char *src);

static void
pgy_runtime_warn_invalid_channel(const char *op, const char *reason)
{
    fprintf(stderr, "[pgy][channel] %s: %s\n",
            op != NULL ? op : "<op>",
            reason != NULL ? reason : "invalid channel operation");
}

static void
pgy_runtime_warn_invalid_intent_index(const char *op, int32_t index, int32_t count)
{
    fprintf(stderr, "[pgy][intent] %s: invalid index %d (count=%d)\n",
            op != NULL ? op : "<op>", (int)index, (int)count);
}

static void
pgy_runtime_warn_intent_enter_failure(const char *name, const char *reason,
                                      int32_t priority, bool is_concurrent)
{
    fprintf(stderr, "[pgy][intent] enter %s failed: %s (priority=%d concurrent=%s)\n",
            name != NULL ? name : "<intent>",
            reason != NULL ? reason : "unknown reason",
            (int)priority,
            is_concurrent ? "true" : "false");
}

static void
pgy_runtime_warn_invalid_collection(const char *op, const char *reason)
{
    fprintf(stderr, "[pgy][collection] %s: %s\n",
            op != NULL ? op : "<op>",
            reason != NULL ? reason : "invalid collection operation");
}

static bool
pgy_runtime_path_is_absolute(const char *path)
{
    if (path == NULL || path[0] == '\0')
        return false;
#ifdef _WIN32
    if ((path[0] == '/' || path[0] == '\\')
        || (isalpha((unsigned char)path[0]) && path[1] == ':'))
        return true;
#else
    if (path[0] == '/')
        return true;
#endif
    return false;
}

void
pgy_zone_authority_check_export(void *zone_ptr, void *participant_ptr,
                                const char *zone_name,
                                const char *participant_name)
{
    const char *resolved_zone = zone_name != NULL ? zone_name : "<zone>";
    const char *resolved_participant =
        participant_name != NULL ? participant_name : "<participant>";

    if (zone_ptr == NULL) {
        fprintf(stderr,
            "[pgy][authority] zone '%s' entered with null self while validating '%s'\n",
            resolved_zone, resolved_participant);
        abort();
    }
    if (participant_ptr == NULL) {
        fprintf(stderr,
            "[pgy][authority] zone '%s' has null authority participant '%s'\n",
            resolved_zone, resolved_participant);
        abort();
    }
}

static bool
pgy_runtime_path_has_parent_ref(const char *path)
{
    const char *seg = path;

    if (path == NULL)
        return true;

    while (*seg != '\0') {
        const char *end = seg;
        while (*end != '\0' && *end != '/' && *end != '\\')
            end++;
        if ((size_t)(end - seg) == 2 && seg[0] == '.' && seg[1] == '.')
            return true;
        if (*end == '\0')
            break;
        seg = end + 1;
    }

    return false;
}

static bool
pgy_runtime_path_is_within_root(const char *path, const char *root)
{
    size_t root_len;

    if (path == NULL || root == NULL || root[0] == '\0')
        return false;

    root_len = strlen(root);
    if (strncmp(path, root, root_len) != 0)
        return false;
    return path[root_len] == '\0'
        || path[root_len] == '/'
        || path[root_len] == '\\';
}

static bool
pgy_runtime_file_path_allowed(const char *path)
{
    const char *root;
    const char *allow_abs;

    if (path == NULL || path[0] == '\0')
        return false;
    if (pgy_runtime_path_has_parent_ref(path))
        return false;

    root = getenv("PGY_IO_ROOT");
    if (root != NULL && root[0] != '\0') {
        if (pgy_runtime_path_is_absolute(path))
            return pgy_runtime_path_is_within_root(path, root);
        return true;
    }

    if (!pgy_runtime_path_is_absolute(path))
        return true;

    allow_abs = getenv("PGY_IO_ALLOW_ABSOLUTE");
    return allow_abs != NULL && strcmp(allow_abs, "1") == 0;
}

static char *
pgy_runtime_path_join_dup(const char *dir, const char *path)
{
    size_t dlen;
    size_t plen;
    bool needs_sep;
    char *result;

    if (dir == NULL || path == NULL)
        return NULL;

    dlen = strlen(dir);
    plen = strlen(path);
    needs_sep = dlen > 0 && dir[dlen - 1] != '/' && dir[dlen - 1] != '\\';
    result = (char *)malloc(dlen + (needs_sep ? 1 : 0) + plen + 1);
    if (result == NULL)
        return NULL;

    memcpy(result, dir, dlen);
    if (needs_sep)
        result[dlen++] = '/';
    memcpy(result + dlen, path, plen + 1);
    return result;
}

#ifdef _WIN32
static void
pgy_runtime_normalize_path_separators(char *path)
{
    if (path == NULL)
        return;
    while (*path != '\0') {
        if (*path == '\\')
            *path = '/';
        path++;
    }
}
#endif

static char *
pgy_runtime_path_dirname_dup(const char *path)
{
    const char *last_sep;
    const char *last_bsep;
    size_t len;
    char *dir;

    if (path == NULL)
        return NULL;

    last_sep = strrchr(path, '/');
    last_bsep = strrchr(path, '\\');
    if (last_bsep != NULL && (last_sep == NULL || last_bsep > last_sep))
        last_sep = last_bsep;

    if (last_sep == NULL)
        return pgy_runtime_lib_strdup(".");

    len = (size_t)(last_sep - path);
    dir = (char *)malloc(len + 1);
    if (dir == NULL)
        return NULL;
    memcpy(dir, path, len);
    dir[len] = '\0';
    return dir;
}

static bool
pgy_runtime_prefix_match_path(const char *root, const char *path)
{
    if (root == NULL || path == NULL)
        return false;
    {
        size_t i = 0;
        while (root[i] != '\0') {
            char a = root[i];
            char b = path[i];
#ifdef _WIN32
            if (a == '\\')
                a = '/';
            if (b == '\\')
                b = '/';
            if (tolower((unsigned char)a) != tolower((unsigned char)b))
                return false;
#else
            if (a != b)
                return false;
#endif
            i++;
        }
        return path[i] == '\0'
            || path[i] == '/'
            || path[i] == '\\';
    }
}

static char *
pgy_runtime_normalize_full_path_dup(const char *path)
{
    if (path == NULL || path[0] == '\0')
        return NULL;
#ifdef _WIN32
    DWORD needed = GetFullPathNameA(path, 0, NULL, NULL);
    char *buf;

    if (needed == 0)
        return NULL;
    buf = (char *)malloc((size_t)needed + 2);
    if (buf == NULL)
        return NULL;
    if (GetFullPathNameA(path, needed + 1, buf, NULL) == 0) {
        free(buf);
        return NULL;
    }
    pgy_runtime_normalize_path_separators(buf);
    return buf;
#else
    char resolved[PATH_MAX];
    if (realpath(path, resolved) == NULL)
        return NULL;
    return pgy_runtime_lib_strdup(resolved);
#endif
}

#ifdef _WIN32
static bool
pgy_runtime_path_has_reparse_component(const char *path)
{
    char *full;
    size_t len;
    size_t i;

    full = pgy_runtime_normalize_full_path_dup(path);
    if (full == NULL)
        return true;

    len = strlen(full);
    for (i = 0; i <= len; ++i) {
        char saved = full[i];
        DWORD attrs;

        if (saved != '/' && saved != '\0')
            continue;
        full[i] = '\0';
        if (full[0] != '\0'
            && !(isalpha((unsigned char)full[0]) && full[1] == ':' && full[2] == '\0')) {
            attrs = GetFileAttributesA(full);
            if (attrs != INVALID_FILE_ATTRIBUTES
                && (attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
                free(full);
                return true;
            }
        }
        full[i] = saved;
    }

    free(full);
    return false;
}
#endif

static char *
pgy_runtime_resolve_file_path(const char *path, bool for_write)
{
    const char *root = getenv("PGY_IO_ROOT");
    const char *allow_abs;
    char *candidate = NULL;

    if (!pgy_runtime_file_path_allowed(path))
        return NULL;

    if (root != NULL && root[0] != '\0') {
        if (pgy_runtime_path_is_absolute(path))
            candidate = pgy_runtime_lib_strdup(path);
        else
            candidate = pgy_runtime_path_join_dup(root, path);
    } else if (pgy_runtime_path_is_absolute(path)) {
        allow_abs = getenv("PGY_IO_ALLOW_ABSOLUTE");
        if (allow_abs == NULL || strcmp(allow_abs, "1") != 0)
            return NULL;
        candidate = pgy_runtime_lib_strdup(path);
    } else {
        candidate = pgy_runtime_lib_strdup(path);
    }

#ifndef _WIN32
    if (candidate != NULL && root != NULL && root[0] != '\0') {
        char *root_real;
        char *check_real;
        char *check_path = NULL;

        root_real = pgy_runtime_normalize_full_path_dup(root);
        if (root_real == NULL) {
            free(candidate);
            return NULL;
        }

        if (for_write)
            check_path = pgy_runtime_path_dirname_dup(candidate);
        else
            check_path = pgy_runtime_lib_strdup(candidate);

        check_real = check_path != NULL
            ? pgy_runtime_normalize_full_path_dup(check_path)
            : NULL;

        if (check_real == NULL
            || !pgy_runtime_prefix_match_path(root_real, check_real)) {
            free(check_path);
            free(check_real);
            free(root_real);
            free(candidate);
            return NULL;
        }

        free(check_path);
        free(check_real);
        free(root_real);
    }
#else
    if (candidate != NULL && root != NULL && root[0] != '\0') {
        char *root_real = pgy_runtime_normalize_full_path_dup(root);
        char *check_path = NULL;
        char *check_real = NULL;
        char *candidate_real = NULL;

        if (root_real == NULL) {
            free(candidate);
            return NULL;
        }

        if (for_write)
            check_path = pgy_runtime_path_dirname_dup(candidate);
        else
            check_path = pgy_runtime_lib_strdup(candidate);

        check_real = check_path != NULL
            ? pgy_runtime_normalize_full_path_dup(check_path)
            : NULL;
        candidate_real = pgy_runtime_normalize_full_path_dup(candidate);

        if (check_real == NULL
            || candidate_real == NULL
            || !pgy_runtime_prefix_match_path(root_real, check_real)
            || pgy_runtime_path_has_reparse_component(check_path)) {
            free(candidate_real);
            free(check_real);
            free(check_path);
            free(root_real);
            free(candidate);
            return NULL;
        }

        free(candidate);
        candidate = candidate_real;
        free(check_path);
        free(check_real);
        free(root_real);
    } else if (candidate != NULL && pgy_runtime_path_is_absolute(candidate)) {
        char *normalized = pgy_runtime_normalize_full_path_dup(candidate);
        if (normalized == NULL) {
            free(candidate);
            return NULL;
        }
        free(candidate);
        candidate = normalized;
    }
#endif

    return candidate;
}

/* =================================================================
 * Log functions
 * ================================================================= */

void pgy_log_int(int32_t v)    { printf("%d\n", v); }
void pgy_log_long(int64_t v)   { printf("%lld\n", (long long)v); }
void pgy_log_float(float v)    { printf("%f\n", v); }
void pgy_log_double(double v)  { printf("%lf\n", v); }
void pgy_log_bool(bool v)      { printf("%s\n", v ? "true" : "false"); }
void
pgy_log_string(const char *v)
{
    size_t len;

    if (v == NULL)
        v = "(null)";

    fputs(v, stdout);
    len = strlen(v);
    if (len == 0 || v[len - 1] != '\n')
        fputc('\n', stdout);
    fflush(stdout);
}

void
pgy_log_banner(const char *v)
{
    pgy_log_string(v);
}

int32_t
pgy_now_ms(void)
{
#ifdef _WIN32
    return (int32_t)GetTickCount64();
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return 0;
    return (int32_t)((ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000LL));
#endif
}

void
pgy_sleep_ms(int32_t ms)
{
    if (ms <= 0)
        return;
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (long)((ms % 1000) * 1000000L);
    while (nanosleep(&req, &req) != 0 && errno == EINTR) {
    }
#endif
}

static char *pgy_runtime_strdup_export(const char *src);

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

typedef struct {
    void   *data;
    size_t  count;
    size_t  capacity;
} PgyListRaw;

typedef struct {
    void   *data;
    size_t  head;
    size_t  tail;
    size_t  count;
    size_t  capacity;
} PgyQueueRaw;

typedef struct {
    char    **keys;
    void     *values;
    uint8_t  *occupied;
    size_t    count;
    size_t    capacity;
} PgyHashMapRaw;

static uint32_t
pgy_hash_string_export(const char *s)
{
    uint32_t h = 2166136261u;
    if (s == NULL)
        return 0;
    while (*s != '\0') {
        h ^= (uint8_t)(*s++);
        h *= 16777619u;
    }
    return h;
}

void
pgy_list_new_raw_export(void *list_ptr, int64_t elem_size)
{
    PgyListRaw *list = (PgyListRaw *)list_ptr;
    if (list == NULL) {
        pgy_runtime_warn_invalid_collection("list_new", "null list");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("list_new", "non-positive element size");
        return;
    }
    list->capacity = 16;
    list->count = 0;
    list->data = calloc((size_t)list->capacity, (size_t)elem_size);
}

void
pgy_list_push_raw_export(void *list_ptr, void *value_ptr, int64_t elem_size)
{
    PgyListRaw *list = (PgyListRaw *)list_ptr;
    char *dst;
    if (list == NULL) {
        pgy_runtime_warn_invalid_collection("list_push", "null list");
        return;
    }
    if (value_ptr == NULL) {
        pgy_runtime_warn_invalid_collection("list_push", "null value");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("list_push", "non-positive element size");
        return;
    }
    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity == 0 ? 16 : list->capacity * 2;
        void *grown = realloc(list->data, new_capacity * (size_t)elem_size);
        if (grown == NULL) {
            pgy_runtime_warn_invalid_collection("list_push", "realloc failed");
            return;
        }
        list->data = grown;
        list->capacity = new_capacity;
    }
    dst = (char *)list->data + (list->count * (size_t)elem_size);
    memcpy(dst, value_ptr, (size_t)elem_size);
    list->count++;
}

void
pgy_list_get_raw_export(void *list_ptr, int32_t index, void *out_ptr, int64_t elem_size)
{
    PgyListRaw *list = (PgyListRaw *)list_ptr;
    if (out_ptr == NULL) {
        pgy_runtime_warn_invalid_collection("list_get", "null output");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("list_get", "non-positive element size");
        return;
    }
    memset(out_ptr, 0, (size_t)elem_size);
    if (list == NULL) {
        pgy_runtime_warn_invalid_collection("list_get", "null list");
        return;
    }
    if (index < 0 || (size_t)index >= list->count) {
        pgy_runtime_warn_invalid_collection("list_get", "index out of bounds");
        return;
    }
    memcpy(out_ptr,
           (char *)list->data + ((size_t)index * (size_t)elem_size),
           (size_t)elem_size);
}

void
pgy_list_set_raw_export(void *list_ptr, int32_t index, void *value_ptr, int64_t elem_size)
{
    PgyListRaw *list = (PgyListRaw *)list_ptr;
    if (list == NULL) {
        pgy_runtime_warn_invalid_collection("list_set", "null list");
        return;
    }
    if (value_ptr == NULL) {
        pgy_runtime_warn_invalid_collection("list_set", "null value");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("list_set", "non-positive element size");
        return;
    }
    if (index < 0 || (size_t)index >= list->count) {
        pgy_runtime_warn_invalid_collection("list_set", "index out of bounds");
        return;
    }
    memcpy((char *)list->data + ((size_t)index * (size_t)elem_size),
           value_ptr, (size_t)elem_size);
}

int32_t
pgy_list_size_raw_export(void *list_ptr)
{
    PgyListRaw *list = (PgyListRaw *)list_ptr;
    if (list == NULL) {
        pgy_runtime_warn_invalid_collection("list_size", "null list");
        return 0;
    }
    return (int32_t)list->count;
}

void
pgy_list_remove_raw_export(void *list_ptr, int32_t index, int64_t elem_size)
{
    PgyListRaw *list = (PgyListRaw *)list_ptr;
    size_t tail_count;
    if (list == NULL) {
        pgy_runtime_warn_invalid_collection("list_remove", "null list");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("list_remove", "non-positive element size");
        return;
    }
    if (index < 0 || (size_t)index >= list->count) {
        pgy_runtime_warn_invalid_collection("list_remove", "index out of bounds");
        return;
    }
    tail_count = list->count - (size_t)index - 1;
    if (tail_count > 0) {
        memmove((char *)list->data + ((size_t)index * (size_t)elem_size),
                (char *)list->data + (((size_t)index + 1) * (size_t)elem_size),
                tail_count * (size_t)elem_size);
    }
    list->count--;
}

void
pgy_queue_new_raw_export(void *queue_ptr, int64_t elem_size)
{
    PgyQueueRaw *queue = (PgyQueueRaw *)queue_ptr;
    if (queue == NULL) {
        pgy_runtime_warn_invalid_collection("queue_new", "null queue");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("queue_new", "non-positive element size");
        return;
    }
    queue->capacity = 16;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->data = calloc(queue->capacity, (size_t)elem_size);
}

void
pgy_queue_push_raw_export(void *queue_ptr, void *value_ptr, int64_t elem_size)
{
    PgyQueueRaw *queue = (PgyQueueRaw *)queue_ptr;
    if (queue == NULL) {
        pgy_runtime_warn_invalid_collection("queue_push", "null queue");
        return;
    }
    if (value_ptr == NULL) {
        pgy_runtime_warn_invalid_collection("queue_push", "null value");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("queue_push", "non-positive element size");
        return;
    }
    if (queue->count >= queue->capacity) {
        size_t new_capacity = queue->capacity == 0 ? 16 : queue->capacity * 2;
        void *new_data = calloc(new_capacity, (size_t)elem_size);
        if (new_data == NULL) {
            pgy_runtime_warn_invalid_collection("queue_push", "allocation failed");
            return;
        }
        for (size_t i = 0; i < queue->count; i++) {
            memcpy((char *)new_data + (i * (size_t)elem_size),
                   (char *)queue->data + (((queue->head + i) % queue->capacity) * (size_t)elem_size),
                   (size_t)elem_size);
        }
        free(queue->data);
        queue->data = new_data;
        queue->head = 0;
        queue->tail = queue->count;
        queue->capacity = new_capacity;
    }
    memcpy((char *)queue->data + (queue->tail * (size_t)elem_size),
           value_ptr, (size_t)elem_size);
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;
}

void
pgy_queue_pop_raw_export(void *queue_ptr, void *out_ptr, int64_t elem_size)
{
    PgyQueueRaw *queue = (PgyQueueRaw *)queue_ptr;
    if (out_ptr == NULL) {
        pgy_runtime_warn_invalid_collection("queue_pop", "null output");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("queue_pop", "non-positive element size");
        return;
    }
    memset(out_ptr, 0, (size_t)elem_size);
    if (queue == NULL) {
        pgy_runtime_warn_invalid_collection("queue_pop", "null queue");
        return;
    }
    if (queue->count == 0) {
        pgy_runtime_warn_invalid_collection("queue_pop", "empty queue");
        return;
    }
    memcpy(out_ptr,
           (char *)queue->data + (queue->head * (size_t)elem_size),
           (size_t)elem_size);
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
}

int32_t
pgy_queue_size_raw_export(void *queue_ptr)
{
    PgyQueueRaw *queue = (PgyQueueRaw *)queue_ptr;
    if (queue == NULL) {
        pgy_runtime_warn_invalid_collection("queue_size", "null queue");
        return 0;
    }
    return (int32_t)queue->count;
}

bool
pgy_queue_empty_raw_export(void *queue_ptr)
{
    PgyQueueRaw *queue = (PgyQueueRaw *)queue_ptr;
    if (queue == NULL)
        pgy_runtime_warn_invalid_collection("queue_empty", "null queue");
    return queue == NULL || queue->count == 0;
}

void
pgy_map_new_raw_export(void *map_ptr, int64_t value_size)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    if (map == NULL) {
        pgy_runtime_warn_invalid_collection("map_new", "null map");
        return;
    }
    if (value_size <= 0) {
        pgy_runtime_warn_invalid_collection("map_new", "non-positive value size");
        return;
    }
    map->capacity = 16;
    map->count = 0;
    map->keys = (char **)calloc(map->capacity, sizeof(char *));
    map->values = calloc(map->capacity, (size_t)value_size);
    map->occupied = (uint8_t *)calloc(map->capacity, sizeof(uint8_t));
}

static void
pgy_map_grow_raw_export(PgyHashMapRaw *map, int64_t value_size)
{
    size_t old_capacity = map->capacity;
    char **old_keys = map->keys;
    void *old_values = map->values;
    uint8_t *old_occupied = map->occupied;

    map->capacity *= 2;
    map->keys = (char **)calloc(map->capacity, sizeof(char *));
    map->values = calloc(map->capacity, (size_t)value_size);
    map->occupied = (uint8_t *)calloc(map->capacity, sizeof(uint8_t));
    map->count = 0;

    for (size_t i = 0; i < old_capacity; i++) {
        if (!old_occupied[i] || old_keys[i] == NULL)
            continue;
        {
            uint32_t h = pgy_hash_string_export(old_keys[i]) % (uint32_t)map->capacity;
            while (map->occupied[h])
                h = (h + 1) % (uint32_t)map->capacity;
            map->keys[h] = old_keys[i];
            memcpy((char *)map->values + (h * (size_t)value_size),
                   (char *)old_values + (i * (size_t)value_size),
                   (size_t)value_size);
            map->occupied[h] = 1;
            map->count++;
        }
    }

    free(old_keys);
    free(old_values);
    free(old_occupied);
}

void
pgy_map_set_raw_export(void *map_ptr, const char *key, void *value_ptr, int64_t value_size)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    if (map == NULL) {
        pgy_runtime_warn_invalid_collection("map_set", "null map");
        return;
    }
    if (key == NULL) {
        pgy_runtime_warn_invalid_collection("map_set", "null key");
        return;
    }
    if (value_ptr == NULL) {
        pgy_runtime_warn_invalid_collection("map_set", "null value");
        return;
    }
    if (value_size <= 0) {
        pgy_runtime_warn_invalid_collection("map_set", "non-positive value size");
        return;
    }
    if ((double)map->count / (double)map->capacity > 0.75)
        pgy_map_grow_raw_export(map, value_size);
    h = pgy_hash_string_export(key) % (uint32_t)map->capacity;
    while (map->occupied[h]) {
        if (map->keys[h] != NULL && strcmp(map->keys[h], key) == 0) {
            memcpy((char *)map->values + (h * (size_t)value_size),
                   value_ptr, (size_t)value_size);
            return;
        }
        h = (h + 1) % (uint32_t)map->capacity;
    }
    map->keys[h] = pgy_runtime_strdup_export(key);
    memcpy((char *)map->values + (h * (size_t)value_size),
           value_ptr, (size_t)value_size);
    map->occupied[h] = 1;
    map->count++;
}

void
pgy_map_get_raw_export(void *map_ptr, const char *key, void *out_ptr, int64_t value_size)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (out_ptr == NULL) {
        pgy_runtime_warn_invalid_collection("map_get", "null output");
        return;
    }
    if (value_size <= 0) {
        pgy_runtime_warn_invalid_collection("map_get", "non-positive value size");
        return;
    }
    memset(out_ptr, 0, (size_t)value_size);
    if (map == NULL) {
        pgy_runtime_warn_invalid_collection("map_get", "null map");
        return;
    }
    if (key == NULL) {
        pgy_runtime_warn_invalid_collection("map_get", "null key");
        return;
    }
    if (map->count == 0)
        return;
    h = pgy_hash_string_export(key) % (uint32_t)map->capacity;
    while (map->occupied[h] && probes < map->capacity) {
        if (map->keys[h] != NULL && strcmp(map->keys[h], key) == 0) {
            memcpy(out_ptr,
                   (char *)map->values + (h * (size_t)value_size),
                   (size_t)value_size);
            return;
        }
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
}

bool
pgy_map_has_raw_export(void *map_ptr, const char *key)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (map == NULL) {
        pgy_runtime_warn_invalid_collection("map_has", "null map");
        return false;
    }
    if (key == NULL) {
        pgy_runtime_warn_invalid_collection("map_has", "null key");
        return false;
    }
    if (map->count == 0)
        return false;
    h = pgy_hash_string_export(key) % (uint32_t)map->capacity;
    while (map->occupied[h] && probes < map->capacity) {
        if (map->keys[h] != NULL && strcmp(map->keys[h], key) == 0)
            return true;
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    return false;
}

void
pgy_map_remove_raw_export(void *map_ptr, const char *key, int64_t value_size)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (map == NULL) {
        pgy_runtime_warn_invalid_collection("map_remove", "null map");
        return;
    }
    if (key == NULL) {
        pgy_runtime_warn_invalid_collection("map_remove", "null key");
        return;
    }
    if (value_size <= 0) {
        pgy_runtime_warn_invalid_collection("map_remove", "non-positive value size");
        return;
    }
    if (map->count == 0)
        return;
    h = pgy_hash_string_export(key) % (uint32_t)map->capacity;
    while (map->occupied[h] && probes < map->capacity) {
        if (map->keys[h] != NULL && strcmp(map->keys[h], key) == 0) {
            free(map->keys[h]);
            map->keys[h] = NULL;
            memset((char *)map->values + (h * (size_t)value_size), 0, (size_t)value_size);
            map->occupied[h] = 0;
            map->count--;
            return;
        }
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
}

int32_t
pgy_map_size_raw_export(void *map_ptr)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    if (map == NULL) {
        pgy_runtime_warn_invalid_collection("map_size", "null map");
        return 0;
    }
    return (int32_t)map->count;
}

/* =================================================================
 * Set — raw (type-erased) export functions for LLVM linking
 *
 * Generic hash set using open-addressing with FNV-1a hash on raw bytes.
 * Works for ANY element type (Int, String, Bool, Float, structs)
 * without requiring string conversion.
 *
 * Layout matches PgySet_Generic:
 *   void    *data       — element storage (elem_size * capacity)
 *   uint8_t *occupied   — slot occupancy flags
 *   size_t   count
 *   size_t   capacity
 * ================================================================= */

typedef struct {
    void    *data;
    uint8_t *occupied;
    size_t   count;
    size_t   capacity;
} PgySetRaw;

static uint32_t
pgy_hash_bytes(const void *ptr, size_t len)
{
    const uint8_t *p = (const uint8_t *)ptr;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

static bool
pgy_set_raw_elem_eq(const void *a, const void *b, int64_t elem_size)
{
    /* String comparison for pointer-sized string elements */
    if (elem_size == (int64_t)sizeof(char *)) {
        const char *sa = *(const char *const *)a;
        const char *sb = *(const char *const *)b;
        if (sa == sb) return true;
        if (sa == NULL || sb == NULL) return false;
        return strcmp(sa, sb) == 0;
    }
    return memcmp(a, b, (size_t)elem_size) == 0;
}

static uint32_t
pgy_set_raw_hash(const void *elem, int64_t elem_size)
{
    /* String hashing for pointer-sized string elements */
    if (elem_size == (int64_t)sizeof(char *)) {
        const char *s = *(const char *const *)elem;
        return s != NULL ? pgy_hash_string_export(s) : 0;
    }
    return pgy_hash_bytes(elem, (size_t)elem_size);
}

#define SET_RAW_ELEM(set, idx, esz) ((char *)(set)->data + (idx) * (size_t)(esz))

void
pgy_set_new_raw_export(void *set_ptr, int64_t elem_size)
{
    PgySetRaw *set = (PgySetRaw *)set_ptr;
    if (set == NULL) {
        pgy_runtime_warn_invalid_collection("set_new", "null set");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("set_new", "non-positive element size");
        return;
    }
    set->capacity = 16;
    set->count = 0;
    set->data = calloc(set->capacity, (size_t)elem_size);
    set->occupied = (uint8_t *)calloc(set->capacity, sizeof(uint8_t));
}

static void
pgy_set_raw_rehash(PgySetRaw *set, int64_t elem_size)
{
    size_t oc = set->capacity;
    void *od = set->data;
    uint8_t *oo = set->occupied;
    set->capacity *= 2;
    set->data = calloc(set->capacity, (size_t)elem_size);
    set->occupied = (uint8_t *)calloc(set->capacity, sizeof(uint8_t));
    set->count = 0;
    for (size_t i = 0; i < oc; i++) {
        if (oo[i]) {
            void *elem = (char *)od + i * (size_t)elem_size;
            uint32_t h = pgy_set_raw_hash(elem, elem_size) % (uint32_t)set->capacity;
            while (set->occupied[h]) h = (h + 1) % (uint32_t)set->capacity;
            memcpy(SET_RAW_ELEM(set, h, elem_size), elem, (size_t)elem_size);
            set->occupied[h] = 1;
            set->count++;
        }
    }
    free(od);
    free(oo);
}

void
pgy_set_add_raw_export(void *set_ptr, void *elem_ptr, int64_t elem_size)
{
    PgySetRaw *set = (PgySetRaw *)set_ptr;
    if (set == NULL) {
        pgy_runtime_warn_invalid_collection("set_add", "null set");
        return;
    }
    if (elem_ptr == NULL) {
        pgy_runtime_warn_invalid_collection("set_add", "null element");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("set_add", "non-positive element size");
        return;
    }
    /* Check if already present */
    uint32_t h = pgy_set_raw_hash(elem_ptr, elem_size) % (uint32_t)set->capacity;
    size_t p = 0;
    while (set->occupied[h] && p < set->capacity) {
        if (pgy_set_raw_elem_eq(SET_RAW_ELEM(set, h, elem_size), elem_ptr, elem_size))
            return; /* already in set */
        h = (h + 1) % (uint32_t)set->capacity; p++;
    }
    /* Resize if needed */
    if ((double)set->count / (double)set->capacity > 0.75) {
        pgy_set_raw_rehash(set, elem_size);
        h = pgy_set_raw_hash(elem_ptr, elem_size) % (uint32_t)set->capacity;
        while (set->occupied[h]) h = (h + 1) % (uint32_t)set->capacity;
    }
    memcpy(SET_RAW_ELEM(set, h, elem_size), elem_ptr, (size_t)elem_size);
    set->occupied[h] = 1;
    set->count++;
}

bool
pgy_set_has_raw_export(void *set_ptr, void *elem_ptr, int64_t elem_size)
{
    PgySetRaw *set = (PgySetRaw *)set_ptr;
    if (set == NULL) {
        pgy_runtime_warn_invalid_collection("set_has", "null set");
        return false;
    }
    if (elem_ptr == NULL) {
        pgy_runtime_warn_invalid_collection("set_has", "null element");
        return false;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("set_has", "non-positive element size");
        return false;
    }
    if (set->count == 0)
        return false;
    uint32_t h = pgy_set_raw_hash(elem_ptr, elem_size) % (uint32_t)set->capacity;
    size_t p = 0;
    while (set->occupied[h] && p < set->capacity) {
        if (pgy_set_raw_elem_eq(SET_RAW_ELEM(set, h, elem_size), elem_ptr, elem_size))
            return true;
        h = (h + 1) % (uint32_t)set->capacity; p++;
    }
    return false;
}

void
pgy_set_remove_raw_export(void *set_ptr, void *elem_ptr, int64_t elem_size)
{
    PgySetRaw *set = (PgySetRaw *)set_ptr;
    if (set == NULL) {
        pgy_runtime_warn_invalid_collection("set_remove", "null set");
        return;
    }
    if (elem_ptr == NULL) {
        pgy_runtime_warn_invalid_collection("set_remove", "null element");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("set_remove", "non-positive element size");
        return;
    }
    if (set->count == 0)
        return;
    uint32_t h = pgy_set_raw_hash(elem_ptr, elem_size) % (uint32_t)set->capacity;
    size_t p = 0;
    while (set->occupied[h] && p < set->capacity) {
        if (pgy_set_raw_elem_eq(SET_RAW_ELEM(set, h, elem_size), elem_ptr, elem_size)) {
            memset(SET_RAW_ELEM(set, h, elem_size), 0, (size_t)elem_size);
            set->occupied[h] = 0;
            set->count--;
            return;
        }
        h = (h + 1) % (uint32_t)set->capacity; p++;
    }
}

int32_t
pgy_set_size_raw_export(void *set_ptr)
{
    PgySetRaw *set = (PgySetRaw *)set_ptr;
    if (set == NULL) {
        pgy_runtime_warn_invalid_collection("set_size", "null set");
        return 0;
    }
    return (int32_t)set->count;
}

#define PGY_INTENT_ACTIVE_MAX 256

typedef struct {
    char *name;
    char *zone;
    char *phase;
    char *participant;
    char *slot;
    char *from_zone;
    char *from_slot;
    char *to_zone;
    char *to_slot;
    bool ok;
    char *failure_reason;
} PgyIntentHistoryStep;

#define PGY_INTENT_INLINE_SUBJECT_CAPACITY 4

typedef struct {
    int32_t handle;
    int32_t parent_handle;
    char   *name;
    void  **subjects;
    void   *inline_subjects[PGY_INTENT_INLINE_SUBJECT_CAPACITY];
    int32_t subject_count;
    bool    is_concurrent;
    int32_t priority;
    int32_t trace_id;
    char   *trace;
    char   *failure_reason;
    int32_t step_count;
    bool    failed;
    PgyIntentHistoryStep steps[PGY_INTENT_ACTIVE_MAX];
    bool    active;
} PgyIntentActiveEntry;

static PgyIntentActiveEntry pgy_intent_active_registry[PGY_INTENT_ACTIVE_MAX];
static pthread_mutex_t pgy_intent_registry_mutex = PTHREAD_MUTEX_INITIALIZER;
static _Thread_local int32_t pgy_intent_current_stack[PGY_INTENT_ACTIVE_MAX];
static _Thread_local int32_t pgy_intent_current_depth = 0;
static int32_t pgy_intent_next_handle = 1;
static int32_t pgy_intent_next_trace_id = 1;
static char *pgy_intent_last_trace = NULL;
static char *pgy_intent_last_failure = NULL;
static char *pgy_intent_last_name = NULL;
static int32_t pgy_intent_last_handle = 0;
static int32_t pgy_intent_last_trace_id = 0;
static int32_t pgy_intent_last_step_count = 0;
static bool pgy_intent_last_failed = false;
static PgyIntentHistoryStep pgy_intent_last_steps[PGY_INTENT_ACTIVE_MAX];
static int32_t pgy_intent_last_history_count = 0;

static char *
pgy_runtime_strdup_export(const char *src)
{
    size_t len;
    char *copy;

    if (src == NULL)
        src = "";
    len = strlen(src);
    copy = (char *)malloc(len + 1);
    if (copy == NULL)
        return NULL;
    memcpy(copy, src, len + 1);
    return copy;
}

static void
pgy_intent_history_step_set_string_export(char **dst, const char *value)
{
    if (dst == NULL)
        return;
    free(*dst);
    *dst = pgy_runtime_strdup_export(value != NULL ? value : "");
}

static void
pgy_intent_history_step_clear_export(PgyIntentHistoryStep *step)
{
    if (step == NULL)
        return;
    free(step->name);
    free(step->zone);
    free(step->phase);
    free(step->participant);
    free(step->slot);
    free(step->from_zone);
    free(step->from_slot);
    free(step->to_zone);
    free(step->to_slot);
    free(step->failure_reason);
    step->name = NULL;
    step->zone = NULL;
    step->phase = NULL;
    step->participant = NULL;
    step->slot = NULL;
    step->from_zone = NULL;
    step->from_slot = NULL;
    step->to_zone = NULL;
    step->to_slot = NULL;
    step->failure_reason = NULL;
    step->ok = false;
}

static void
pgy_intent_append_line_export(char **dst, const char *line)
{
    size_t old_len = 0;
    size_t add_len = 0;
    char *grown;

    if (dst == NULL || line == NULL)
        return;
    if (*dst != NULL)
        old_len = strlen(*dst);
    add_len = strlen(line);
    grown = (char *)realloc(*dst, old_len + add_len + 1);
    if (grown == NULL)
        return;
    memcpy(grown + old_len, line, add_len + 1);
    *dst = grown;
}

static PgyIntentActiveEntry *
pgy_intent_find_active_entry_export(int32_t handle)
{
    for (int i = 0; i < PGY_INTENT_ACTIVE_MAX; i++) {
        if (pgy_intent_active_registry[i].active
            && pgy_intent_active_registry[i].handle == handle) {
            return &pgy_intent_active_registry[i];
        }
    }
    return NULL;
}

static int32_t
pgy_intent_current_handle_export(void)
{
    if (pgy_intent_current_depth <= 0)
        return 0;
    return pgy_intent_current_stack[pgy_intent_current_depth - 1];
}

static bool
pgy_intent_handle_is_current_ancestor_export(int32_t handle)
{
    int32_t cursor = pgy_intent_current_handle_export();

    while (cursor != 0) {
        PgyIntentActiveEntry *entry;

        if (cursor == handle)
            return true;
        entry = pgy_intent_find_active_entry_export(cursor);
        if (entry == NULL || entry->parent_handle == cursor)
            break;
        cursor = entry->parent_handle;
    }
    return false;
}

static void
pgy_intent_push_current_handle_export(int32_t handle)
{
    if (handle == 0 || pgy_intent_current_depth >= PGY_INTENT_ACTIVE_MAX)
        return;
    pgy_intent_current_stack[pgy_intent_current_depth++] = handle;
}

static void
pgy_intent_pop_current_handle_export(int32_t handle)
{
    for (int32_t i = pgy_intent_current_depth - 1; i >= 0; i--) {
        if (pgy_intent_current_stack[i] != handle)
            continue;
        for (int32_t j = i; j + 1 < pgy_intent_current_depth; j++)
            pgy_intent_current_stack[j] = pgy_intent_current_stack[j + 1];
        pgy_intent_current_depth--;
        return;
    }
}

static bool
pgy_intent_subjects_overlap_export(void **lhs, int32_t lhs_count,
                                   void **rhs, int32_t rhs_count)
{
    for (int32_t i = 0; i < lhs_count; i++) {
        if (lhs == NULL || lhs[i] == NULL)
            continue;
        for (int32_t j = 0; j < rhs_count; j++) {
            if (rhs == NULL || rhs[j] == NULL)
                continue;
            if (lhs[i] == rhs[j])
                return true;
        }
    }
    return false;
}

int32_t
pgy_intent_enter_export(char *name, void **subjects, int32_t subject_count,
                        bool is_concurrent, int32_t priority)
{
    int free_index = -1;
    int32_t handle = 0;
    int32_t parent_handle = 0;
    void **subject_copy = NULL;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    parent_handle = pgy_intent_current_handle_export();

    for (int i = 0; i < PGY_INTENT_ACTIVE_MAX; i++) {
        PgyIntentActiveEntry *entry = &pgy_intent_active_registry[i];
        if (!entry->active)
            continue;
        if (!pgy_intent_subjects_overlap_export(entry->subjects, entry->subject_count,
                                                subjects, subject_count))
            continue;
        if (pgy_intent_handle_is_current_ancestor_export(entry->handle))
            continue;
        if (entry->is_concurrent && is_concurrent)
            continue;
        if (priority > entry->priority)
            continue;
        pgy_runtime_warn_intent_enter_failure(name,
            "same-subject conflict with active intent",
            priority, is_concurrent);
        pthread_mutex_unlock(&pgy_intent_registry_mutex);
        return 0;
    }

    for (int i = 0; i < PGY_INTENT_ACTIVE_MAX; i++) {
        if (!pgy_intent_active_registry[i].active) {
            free_index = i;
            break;
        }
    }

    if (free_index < 0) {
        pgy_runtime_warn_intent_enter_failure(name,
            "active registry capacity exhausted",
            priority, is_concurrent);
        pthread_mutex_unlock(&pgy_intent_registry_mutex);
        return 0;
    }

    if (subject_count > 0) {
        if (subject_count <= PGY_INTENT_INLINE_SUBJECT_CAPACITY) {
            subject_copy = pgy_intent_active_registry[free_index].inline_subjects;
        } else {
            subject_copy = (void **)malloc(sizeof(void *) * (size_t)subject_count);
            if (subject_copy == NULL) {
                pgy_runtime_warn_intent_enter_failure(name,
                    "subject registry allocation failed",
                    priority, is_concurrent);
                pthread_mutex_unlock(&pgy_intent_registry_mutex);
                return 0;
            }
        }
        memcpy(subject_copy, subjects, sizeof(void *) * (size_t)subject_count);
    }

    handle = pgy_intent_next_handle++;
    pgy_intent_active_registry[free_index].handle = handle;
    pgy_intent_active_registry[free_index].parent_handle = parent_handle;
    pgy_intent_active_registry[free_index].name = PGY_INTENT_OBSERVABILITY_ENABLED
        ? pgy_runtime_strdup_export(name)
        : NULL;
    pgy_intent_active_registry[free_index].subjects = subject_copy;
    pgy_intent_active_registry[free_index].subject_count = subject_count;
    pgy_intent_active_registry[free_index].is_concurrent = is_concurrent;
    pgy_intent_active_registry[free_index].priority = priority;
    pgy_intent_active_registry[free_index].trace_id = PGY_INTENT_OBSERVABILITY_ENABLED
        ? pgy_intent_next_trace_id++ : 0;
    pgy_intent_active_registry[free_index].trace = NULL;
    pgy_intent_active_registry[free_index].failure_reason = NULL;
    pgy_intent_active_registry[free_index].step_count = 0;
    pgy_intent_active_registry[free_index].failed = false;
    pgy_intent_active_registry[free_index].active = true;
    if (PGY_INTENT_OBSERVABILITY_ENABLED) {
        char line[256];
        snprintf(line, sizeof(line), "[intent] enter %s\n",
            name != NULL ? name : "<intent>");
        pgy_intent_append_line_export(&pgy_intent_active_registry[free_index].trace, line);
    }
    pgy_intent_push_current_handle_export(handle);

    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return handle;
}

void
pgy_intent_trace_step_export(int32_t handle, char *step_name, char *zone_name)
{
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_find_active_entry_export(handle);
    if (entry != NULL) {
        entry->step_count++;
        if (!PGY_INTENT_OBSERVABILITY_ENABLED) {
            pthread_mutex_unlock(&pgy_intent_registry_mutex);
            return;
        }
        char line[256];
        snprintf(line, sizeof(line), "[step] begin %s @ %s\n",
            step_name != NULL ? step_name : "<step>",
            zone_name != NULL ? zone_name : "<zone>");
        pgy_intent_append_line_export(&entry->trace, line);
        if (entry->step_count <= PGY_INTENT_ACTIVE_MAX) {
            int32_t index = entry->step_count - 1;
            pgy_intent_history_step_clear_export(&entry->steps[index]);
            entry->steps[index].name = pgy_runtime_strdup_export(step_name != NULL ? step_name : "");
            entry->steps[index].zone = pgy_runtime_strdup_export(zone_name != NULL ? zone_name : "");
            entry->steps[index].phase = pgy_runtime_strdup_export("begin");
            entry->steps[index].participant = pgy_runtime_strdup_export("");
            entry->steps[index].slot = pgy_runtime_strdup_export("");
            entry->steps[index].from_zone = pgy_runtime_strdup_export("");
            entry->steps[index].from_slot = pgy_runtime_strdup_export("");
            entry->steps[index].to_zone = pgy_runtime_strdup_export("");
            entry->steps[index].to_slot = pgy_runtime_strdup_export("");
            entry->steps[index].ok = false;
            entry->steps[index].failure_reason = pgy_runtime_strdup_export("");
        }
    }
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

void
pgy_intent_trace_bind_export(int32_t handle, char *participant_name, char *slot_name)
{
    if (!PGY_INTENT_OBSERVABILITY_ENABLED)
        return;
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_find_active_entry_export(handle);
    if (entry != NULL) {
        char line[256];
        snprintf(line, sizeof(line), "[bind] %s -> %s\n",
            participant_name != NULL ? participant_name : "<participant>",
            slot_name != NULL ? slot_name : "<unbound>");
        pgy_intent_append_line_export(&entry->trace, line);
        if (entry->step_count > 0 && entry->step_count <= PGY_INTENT_ACTIVE_MAX) {
            int32_t index = entry->step_count - 1;
            pgy_intent_history_step_set_string_export(&entry->steps[index].participant, participant_name);
            pgy_intent_history_step_set_string_export(&entry->steps[index].slot, slot_name);
        }
    }
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

void
pgy_intent_trace_materialize_export(int32_t handle, char *participant_name,
                                    char *slot_name, char *zone_name)
{
    if (!PGY_INTENT_OBSERVABILITY_ENABLED)
        return;
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_find_active_entry_export(handle);
    if (entry != NULL) {
        char line[320];
        snprintf(line, sizeof(line), "[materialize] %s => %s.%s\n",
            participant_name != NULL ? participant_name : "<participant>",
            zone_name != NULL ? zone_name : "<zone>",
            slot_name != NULL ? slot_name : "<unbound>");
        pgy_intent_append_line_export(&entry->trace, line);
        if (entry->step_count > 0 && entry->step_count <= PGY_INTENT_ACTIVE_MAX) {
            int32_t index = entry->step_count - 1;
            pgy_intent_history_step_set_string_export(&entry->steps[index].phase, "materialize");
            pgy_intent_history_step_set_string_export(&entry->steps[index].participant, participant_name);
            pgy_intent_history_step_set_string_export(&entry->steps[index].slot, slot_name);
            pgy_intent_history_step_set_string_export(&entry->steps[index].to_zone, zone_name);
            pgy_intent_history_step_set_string_export(&entry->steps[index].to_slot, slot_name);
        }
    }
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

void
pgy_intent_trace_transfer_export(int32_t handle, char *participant_name,
                                 char *from_zone_name, char *from_slot_name,
                                 char *to_zone_name, char *to_slot_name)
{
    if (!PGY_INTENT_OBSERVABILITY_ENABLED)
        return;
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_find_active_entry_export(handle);
    if (entry != NULL) {
        char line[384];
        snprintf(line, sizeof(line), "[transfer] %s: %s.%s -> %s.%s\n",
            participant_name != NULL ? participant_name : "<participant>",
            from_zone_name != NULL ? from_zone_name : "<zone>",
            from_slot_name != NULL ? from_slot_name : "<unbound>",
            to_zone_name != NULL ? to_zone_name : "<zone>",
            to_slot_name != NULL ? to_slot_name : "<unbound>");
        pgy_intent_append_line_export(&entry->trace, line);
        if (entry->step_count > 0 && entry->step_count <= PGY_INTENT_ACTIVE_MAX) {
            int32_t index = entry->step_count - 1;
            pgy_intent_history_step_set_string_export(&entry->steps[index].phase, "transfer");
            pgy_intent_history_step_set_string_export(&entry->steps[index].participant, participant_name);
            pgy_intent_history_step_set_string_export(&entry->steps[index].from_zone, from_zone_name);
            pgy_intent_history_step_set_string_export(&entry->steps[index].from_slot, from_slot_name);
            pgy_intent_history_step_set_string_export(&entry->steps[index].to_zone, to_zone_name);
            pgy_intent_history_step_set_string_export(&entry->steps[index].to_slot, to_slot_name);
        }
    }
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

void
pgy_intent_trace_step_ok_export(int32_t handle, char *step_name)
{
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_find_active_entry_export(handle);
    if (entry != NULL) {
        if (entry->step_count > 0 && entry->step_count <= PGY_INTENT_ACTIVE_MAX)
            entry->steps[entry->step_count - 1].ok = true;
        if (!PGY_INTENT_OBSERVABILITY_ENABLED) {
            pthread_mutex_unlock(&pgy_intent_registry_mutex);
            return;
        }
        char line[256];
        snprintf(line, sizeof(line), "[step] ok %s\n",
            step_name != NULL ? step_name : "<step>");
        pgy_intent_append_line_export(&entry->trace, line);
        if (entry->step_count > 0 && entry->step_count <= PGY_INTENT_ACTIVE_MAX) {
            pgy_intent_history_step_set_string_export(
                &entry->steps[entry->step_count - 1].phase, "ok");
        }
    }
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

void
pgy_intent_trace_fail_export(int32_t handle, char *reason)
{
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_find_active_entry_export(handle);
    if (entry != NULL) {
        entry->failed = true;
        if (!PGY_INTENT_OBSERVABILITY_ENABLED) {
            pthread_mutex_unlock(&pgy_intent_registry_mutex);
            return;
        }
        char line[256];
        free(entry->failure_reason);
        entry->failure_reason = pgy_runtime_strdup_export(reason != NULL ? reason : "");
        if (entry->step_count > 0 && entry->step_count <= PGY_INTENT_ACTIVE_MAX) {
            int32_t index = entry->step_count - 1;
            pgy_intent_history_step_set_string_export(&entry->steps[index].phase, "fail");
            pgy_intent_history_step_set_string_export(&entry->steps[index].failure_reason, reason);
        }
        snprintf(line, sizeof(line), "[fail] %s\n",
            reason != NULL ? reason : "<failure>");
        pgy_intent_append_line_export(&entry->trace, line);
    }
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

void
pgy_mir_resource_op_export(int32_t handle,
                           const char *op_name,
                           const char *slot_anchor,
                           const char *arg_name)
{
#ifdef PGY_MIR_TRACE
    fprintf(stderr, "[MIR resource-op] handle=%d op=%s slot=%s arg=%s\n",
            handle,
            op_name      != NULL ? op_name      : "-",
            slot_anchor  != NULL ? slot_anchor  : "-",
            arg_name     != NULL ? arg_name     : "-");
#else
    (void)handle;
    (void)op_name;
    (void)slot_anchor;
    (void)arg_name;
#endif
}

void
pgy_mir_cleanup_op_export(int32_t handle,
                          const char *op_name,
                          const char *slot_anchor,
                          const char *arg_name)
{
#ifdef PGY_MIR_TRACE
    fprintf(stderr, "[MIR cleanup-op] handle=%d op=%s slot=%s arg=%s\n",
            handle,
            op_name      != NULL ? op_name      : "-",
            slot_anchor  != NULL ? slot_anchor  : "-",
            arg_name     != NULL ? arg_name     : "-");
#else
    (void)handle;
    (void)op_name;
    (void)slot_anchor;
    (void)arg_name;
#endif
}

char *
pgy_intent_last_trace_export(void)
{
    return pgy_intent_last_trace != NULL ? pgy_intent_last_trace : "";
}

char *
pgy_intent_last_failure_export(void)
{
    return pgy_intent_last_failure != NULL ? pgy_intent_last_failure : "";
}

char *
pgy_intent_last_name_export(void)
{
    return pgy_intent_last_name != NULL ? pgy_intent_last_name : "";
}

int32_t
pgy_intent_last_handle_export(void)
{
    return pgy_intent_last_handle;
}

int32_t
pgy_intent_last_trace_id_export(void)
{
    return pgy_intent_last_trace_id;
}

int32_t
pgy_intent_last_step_count_export(void)
{
    return pgy_intent_last_step_count;
}

bool
pgy_intent_last_failed_export(void)
{
    return pgy_intent_last_failed;
}

int32_t
pgy_intent_history_count_export(void)
{
    return pgy_intent_last_history_count;
}

static PgyIntentActiveEntry *
pgy_intent_active_entry_by_index_export(int32_t index)
{
    int32_t seen = 0;

    if (index < 0) {
        pgy_runtime_warn_invalid_intent_index("active_entry_by_index", index, -1);
        return NULL;
    }

    for (int i = 0; i < PGY_INTENT_ACTIVE_MAX; i++) {
        if (!pgy_intent_active_registry[i].active)
            continue;
        if (seen == index)
            return &pgy_intent_active_registry[i];
        seen++;
    }

    pgy_runtime_warn_invalid_intent_index("active_entry_by_index", index, seen);
    return NULL;
}

char *
pgy_intent_history_step_name_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_name", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].name != NULL ? pgy_intent_last_steps[index].name : "";
}

char *
pgy_intent_history_step_zone_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_zone", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].zone != NULL ? pgy_intent_last_steps[index].zone : "";
}

char *
pgy_intent_history_step_phase_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_phase", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].phase != NULL ? pgy_intent_last_steps[index].phase : "";
}

char *
pgy_intent_history_step_participant_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_participant", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].participant != NULL ? pgy_intent_last_steps[index].participant : "";
}

char *
pgy_intent_history_step_slot_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_slot", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].slot != NULL ? pgy_intent_last_steps[index].slot : "";
}

char *
pgy_intent_history_step_from_zone_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_from_zone", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].from_zone != NULL ? pgy_intent_last_steps[index].from_zone : "";
}

char *
pgy_intent_history_step_from_slot_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_from_slot", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].from_slot != NULL ? pgy_intent_last_steps[index].from_slot : "";
}

char *
pgy_intent_history_step_to_zone_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_to_zone", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].to_zone != NULL ? pgy_intent_last_steps[index].to_zone : "";
}

char *
pgy_intent_history_step_to_slot_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_to_slot", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].to_slot != NULL ? pgy_intent_last_steps[index].to_slot : "";
}

bool
pgy_intent_history_step_ok_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_ok", index,
                                              pgy_intent_last_history_count);
        return false;
    }
    return pgy_intent_last_steps[index].ok;
}

char *
pgy_intent_history_step_failure_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_failure", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].failure_reason != NULL
        ? pgy_intent_last_steps[index].failure_reason : "";
}

int32_t
pgy_intent_active_count_export(void)
{
    int32_t count = 0;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    for (int i = 0; i < PGY_INTENT_ACTIVE_MAX; i++) {
        if (pgy_intent_active_registry[i].active)
            count++;
    }
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return count;
}

char *
pgy_intent_active_name_export(int32_t index)
{
    char *result = "";

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_index_export(index);
    if (entry != NULL && entry->name != NULL)
        result = entry->name;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

int32_t
pgy_intent_active_handle_export(int32_t index)
{
    int32_t result = 0;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_index_export(index);
    if (entry != NULL)
        result = entry->handle;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

int32_t
pgy_intent_active_priority_export(int32_t index)
{
    int32_t result = 0;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_index_export(index);
    if (entry != NULL)
        result = entry->priority;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

int32_t
pgy_intent_active_trace_id_export(int32_t index)
{
    int32_t result = 0;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_index_export(index);
    if (entry != NULL)
        result = entry->trace_id;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

bool
pgy_intent_active_concurrent_export(int32_t index)
{
    bool result = false;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_index_export(index);
    if (entry != NULL)
        result = entry->is_concurrent;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

char *
pgy_intent_active_trace_export(int32_t index)
{
    char *result = "";

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_index_export(index);
    if (entry != NULL && entry->trace != NULL)
        result = entry->trace;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

int32_t
pgy_intent_active_parent_handle_export(int32_t index)
{
    int32_t result = 0;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_index_export(index);
    if (entry != NULL)
        result = entry->parent_handle;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

int32_t
pgy_intent_active_subject_count_export(int32_t index)
{
    int32_t result = 0;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_index_export(index);
    if (entry != NULL)
        result = entry->subject_count;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

int32_t
pgy_intent_active_step_count_export(int32_t index)
{
    int32_t result = 0;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_index_export(index);
    if (entry != NULL)
        result = entry->step_count;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

bool
pgy_intent_active_failed_export(int32_t index)
{
    bool result = false;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_index_export(index);
    if (entry != NULL)
        result = entry->failed;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

char *
pgy_intent_active_failure_export(int32_t index)
{
    char *result = "";

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_index_export(index);
    if (entry != NULL && entry->failure_reason != NULL)
        result = entry->failure_reason;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

void
pgy_intent_exit_export(int32_t handle)
{
    if (handle == 0)
        return;

    pgy_intent_pop_current_handle_export(handle);
    pthread_mutex_lock(&pgy_intent_registry_mutex);

    for (int i = 0; i < PGY_INTENT_ACTIVE_MAX; i++) {
        PgyIntentActiveEntry *entry = &pgy_intent_active_registry[i];
        if (!entry->active || entry->handle != handle)
            continue;
        if (PGY_INTENT_OBSERVABILITY_ENABLED) {
            free(pgy_intent_last_trace);
            free(pgy_intent_last_failure);
            free(pgy_intent_last_name);
            for (int32_t j = 0; j < pgy_intent_last_history_count; j++) {
                free(pgy_intent_last_steps[j].name);
                free(pgy_intent_last_steps[j].zone);
                free(pgy_intent_last_steps[j].failure_reason);
                pgy_intent_last_steps[j].name = NULL;
                pgy_intent_last_steps[j].zone = NULL;
                pgy_intent_last_steps[j].failure_reason = NULL;
                pgy_intent_last_steps[j].ok = false;
            }
            pgy_intent_last_trace = entry->trace != NULL
                ? pgy_runtime_strdup_export(entry->trace) : pgy_runtime_strdup_export("");
            pgy_intent_last_failure = entry->failure_reason != NULL
                ? pgy_runtime_strdup_export(entry->failure_reason) : pgy_runtime_strdup_export("");
            pgy_intent_last_name = entry->name != NULL
                ? pgy_runtime_strdup_export(entry->name) : pgy_runtime_strdup_export("");
            pgy_intent_last_handle = entry->handle;
            pgy_intent_last_trace_id = entry->trace_id;
            pgy_intent_last_step_count = entry->step_count;
            pgy_intent_last_failed = entry->failed;
            pgy_intent_last_history_count = entry->step_count;
            if (pgy_intent_last_history_count > PGY_INTENT_ACTIVE_MAX)
                pgy_intent_last_history_count = PGY_INTENT_ACTIVE_MAX;
            for (int32_t j = 0; j < pgy_intent_last_history_count; j++) {
                pgy_intent_history_step_clear_export(&pgy_intent_last_steps[j]);
                pgy_intent_last_steps[j].name = pgy_runtime_strdup_export(
                    entry->steps[j].name != NULL ? entry->steps[j].name : "");
                pgy_intent_last_steps[j].zone = pgy_runtime_strdup_export(
                    entry->steps[j].zone != NULL ? entry->steps[j].zone : "");
                pgy_intent_last_steps[j].phase = pgy_runtime_strdup_export(
                    entry->steps[j].phase != NULL ? entry->steps[j].phase : "");
                pgy_intent_last_steps[j].participant = pgy_runtime_strdup_export(
                    entry->steps[j].participant != NULL ? entry->steps[j].participant : "");
                pgy_intent_last_steps[j].slot = pgy_runtime_strdup_export(
                    entry->steps[j].slot != NULL ? entry->steps[j].slot : "");
                pgy_intent_last_steps[j].from_zone = pgy_runtime_strdup_export(
                    entry->steps[j].from_zone != NULL ? entry->steps[j].from_zone : "");
                pgy_intent_last_steps[j].from_slot = pgy_runtime_strdup_export(
                    entry->steps[j].from_slot != NULL ? entry->steps[j].from_slot : "");
                pgy_intent_last_steps[j].to_zone = pgy_runtime_strdup_export(
                    entry->steps[j].to_zone != NULL ? entry->steps[j].to_zone : "");
                pgy_intent_last_steps[j].to_slot = pgy_runtime_strdup_export(
                    entry->steps[j].to_slot != NULL ? entry->steps[j].to_slot : "");
                pgy_intent_last_steps[j].ok = entry->steps[j].ok;
                pgy_intent_last_steps[j].failure_reason = pgy_runtime_strdup_export(
                    entry->steps[j].failure_reason != NULL ? entry->steps[j].failure_reason : "");
            }
        } else {
            free(pgy_intent_last_trace);
            free(pgy_intent_last_failure);
            free(pgy_intent_last_name);
            pgy_intent_last_trace = NULL;
            pgy_intent_last_failure = NULL;
            pgy_intent_last_name = NULL;
            pgy_intent_last_handle = entry->handle;
            pgy_intent_last_trace_id = 0;
            pgy_intent_last_step_count = entry->step_count;
            pgy_intent_last_failed = entry->failed;
            pgy_intent_last_history_count = 0;
        }
        if (PGY_INTENT_OBSERVABILITY_ENABLED)
            free(entry->name);
        if (entry->subjects != entry->inline_subjects)
            free(entry->subjects);
        free(entry->trace);
        free(entry->failure_reason);
        if (PGY_INTENT_OBSERVABILITY_ENABLED) {
            for (int32_t j = 0; j < entry->step_count && j < PGY_INTENT_ACTIVE_MAX; j++) {
                pgy_intent_history_step_clear_export(&entry->steps[j]);
            }
        }
        entry->handle = 0;
        entry->parent_handle = 0;
        entry->name = NULL;
        entry->subjects = NULL;
        entry->subject_count = 0;
        entry->is_concurrent = false;
        entry->priority = 0;
        entry->trace_id = 0;
        entry->trace = NULL;
        entry->failure_reason = NULL;
        entry->step_count = 0;
        entry->failed = false;
        entry->active = false;
        break;
    }

    pthread_mutex_unlock(&pgy_intent_registry_mutex);
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
    if (s == NULL) return;
    if (!s->claimed) {
        fprintf(stderr, "[pgy] slot write after release (Int)\n");
        return;
    }
    s->value = v;
}

int32_t pgy_read_Int(PgySlot_Int *s)
{
    if (s == NULL) return 0;
    if (!s->claimed) {
        fprintf(stderr, "[pgy] slot read after release (Int)\n");
        return 0;
    }
    return s->value;
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
    if (s == NULL) return;
    if (!s->claimed) { fprintf(stderr, "[pgy] slot write after release (Long)\n"); return; }
    s->value = v;
}

int64_t pgy_read_Long(PgySlot_Long *s)
{
    if (s == NULL) return 0;
    if (!s->claimed) { fprintf(stderr, "[pgy] slot read after release (Long)\n"); return 0; }
    return s->value;
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
    if (s == NULL) return;
    if (!s->claimed) { fprintf(stderr, "[pgy] slot write after release (Float)\n"); return; }
    s->value = v;
}

float pgy_read_Float(PgySlot_Float *s)
{
    if (s == NULL) return 0.0f;
    if (!s->claimed) { fprintf(stderr, "[pgy] slot read after release (Float)\n"); return 0.0f; }
    return s->value;
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
    if (s == NULL) return;
    if (!s->claimed) { fprintf(stderr, "[pgy] slot write after release (Double)\n"); return; }
    s->value = v;
}

double pgy_read_Double(PgySlot_Double *s)
{
    if (s == NULL) return 0.0;
    if (!s->claimed) { fprintf(stderr, "[pgy] slot read after release (Double)\n"); return 0.0; }
    return s->value;
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
    if (s == NULL) return;
    if (!s->claimed) { fprintf(stderr, "[pgy] slot write after release (Bool)\n"); return; }
    s->value = v;
}

bool pgy_read_Bool(PgySlot_Bool *s)
{
    if (s == NULL) return false;
    if (!s->claimed) { fprintf(stderr, "[pgy] slot read after release (Bool)\n"); return false; }
    return s->value;
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
    if (s == NULL) return;
    if (!s->claimed) { fprintf(stderr, "[pgy] slot write after release (String)\n"); return; }
    s->value = v;
}

char *pgy_read_String(PgySlot_String *s)
{
    if (s == NULL) return NULL;
    if (!s->claimed) { fprintf(stderr, "[pgy] slot read after release (String)\n"); return NULL; }
    return s->value;
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
    char *resolved = pgy_runtime_resolve_file_path(path, false);
    if (resolved == NULL)
        return pgy_runtime_lib_strdup("");

    FILE *fp = fopen(resolved, "rb");
    if (fp == NULL)
        return pgy_runtime_lib_strdup("");

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return pgy_runtime_lib_strdup("");
    }
    long len = ftell(fp);
    if (len < 0 || (unsigned long)len > (unsigned long)PGY_RUNTIME_MAX_FILE_BYTES) {
        fclose(fp);
        return pgy_runtime_lib_strdup("");
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return pgy_runtime_lib_strdup("");
    }

    char *buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL) {
        fclose(fp);
        return pgy_runtime_lib_strdup("");
    }

    size_t read_len = fread(buf, 1, (size_t)len, fp);
    if (read_len != (size_t)len) {
        fclose(fp);
        free(resolved);
        free(buf);
        return pgy_runtime_lib_strdup("");
    }
    buf[read_len] = '\0';
    fclose(fp);
    free(resolved);
    return buf;
}

void pgy_write_file(const char *path, const char *data)
{
    char *resolved = pgy_runtime_resolve_file_path(path, true);
    if (resolved == NULL)
        return;
    FILE *fp = fopen(resolved, "wb");
    if (fp == NULL)
        return;
    if (data != NULL) {
        size_t len = strlen(data);
        (void)fwrite(data, 1, len, fp);
    }
    fclose(fp);
    free(resolved);
}

char *pgy_input(const char *prompt)
{
    char tmp[4096];

    if (prompt != NULL && prompt[0] != '\0')
        printf("%s", prompt);
    fflush(stdout);

    tmp[0] = '\0';
    if (fgets(tmp, sizeof(tmp), stdin) == NULL) {
        char *empty = (char *)malloc(1);
        if (empty != NULL) empty[0] = '\0';
        return empty;
    }

    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '\n')
        tmp[len - 1] = '\0';

    len = strlen(tmp);
    char *result = (char *)malloc(len + 1);
    if (result != NULL)
        memcpy(result, tmp, len + 1);
    return result;
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

bool pgy_string_equals(const char *a, const char *b)
{
    if (a == NULL)
        a = "";
    if (b == NULL)
        b = "";
    return strcmp(a, b) == 0;
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

void SeedRandom(int32_t seed)
{
    srand((unsigned int)seed);
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
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("send_Int", "null channel");
        return false;
    }
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
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("try_send_Int", "null channel");
        return false;
    }
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
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("send_timeout_Int", "null channel");
        return false;
    }
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
    if (ch == NULL || out == NULL) {
        pgy_runtime_warn_invalid_channel("recv_Int",
            ch == NULL ? "null channel" : "null output pointer");
        return false;
    }
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
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("ready_Int", "null channel");
        return false;
    }
    pthread_mutex_lock(&ch->mutex);
    bool ready = ch->count > 0;
    pthread_mutex_unlock(&ch->mutex);
    return ready;
}

int32_t pgy_channel_length_Int(PgyChannel_Int_RT *ch)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("length_Int", "null channel");
        return 0;
    }
    pthread_mutex_lock(&ch->mutex);
    int32_t len = (int32_t)ch->count;
    pthread_mutex_unlock(&ch->mutex);
    return len;
}

int32_t pgy_channel_capacity_Int(PgyChannel_Int_RT *ch)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("capacity_Int", "null channel");
        return 0;
    }
    pthread_mutex_lock(&ch->mutex);
    int32_t cap = (int32_t)ch->capacity;
    pthread_mutex_unlock(&ch->mutex);
    return cap;
}

bool pgy_channel_full_Int(PgyChannel_Int_RT *ch)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("full_Int", "null channel");
        return false;
    }
    pthread_mutex_lock(&ch->mutex);
    bool full = ch->count >= ch->capacity;
    pthread_mutex_unlock(&ch->mutex);
    return full;
}

int32_t pgy_channel_space_Int(PgyChannel_Int_RT *ch)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("space_Int", "null channel");
        return 0;
    }
    pthread_mutex_lock(&ch->mutex);
    int32_t space = (int32_t)(ch->capacity - ch->count);
    pthread_mutex_unlock(&ch->mutex);
    return space;
}

bool pgy_channel_closed_Int(PgyChannel_Int_RT *ch)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("closed_Int", "null channel");
        return true;
    }
    pthread_mutex_lock(&ch->mutex);
    bool closed = ch->closed;
    pthread_mutex_unlock(&ch->mutex);
    return closed;
}

bool pgy_channel_try_recv_Int(PgyChannel_Int_RT *ch, int32_t *out)
{
    if (ch == NULL || out == NULL) {
        pgy_runtime_warn_invalid_channel("try_recv_Int",
            ch == NULL ? "null channel" : "null output pointer");
        return false;
    }
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
    if (ch == NULL || out == NULL) {
        pgy_runtime_warn_invalid_channel("recv_timeout_Int",
            ch == NULL ? "null channel" : "null output pointer");
        return false;
    }
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
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("send_String", "null channel");
        return false;
    }
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
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("try_send_String", "null channel");
        return false;
    }
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
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("send_timeout_String", "null channel");
        return false;
    }
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
    if (ch == NULL || out == NULL) {
        pgy_runtime_warn_invalid_channel("recv_String",
            ch == NULL ? "null channel" : "null output pointer");
        return false;
    }
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
    if (ch == NULL || out == NULL) {
        pgy_runtime_warn_invalid_channel("recv_timeout_String",
            ch == NULL ? "null channel" : "null output pointer");
        return false;
    }
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
    if (ch == NULL || out == NULL) {
        pgy_runtime_warn_invalid_channel("try_recv_String",
            ch == NULL ? "null channel" : "null output pointer");
        return false;
    }
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
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("ready_String", "null channel");
        return false;
    }
    pthread_mutex_lock(&ch->mutex);
    bool ready = ch->count > 0;
    pthread_mutex_unlock(&ch->mutex);
    return ready;
}

int32_t pgy_channel_length_String(PgyChannel_String_RT *ch)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("length_String", "null channel");
        return 0;
    }
    pthread_mutex_lock(&ch->mutex);
    int32_t len = (int32_t)ch->count;
    pthread_mutex_unlock(&ch->mutex);
    return len;
}

int32_t pgy_channel_capacity_String(PgyChannel_String_RT *ch)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("capacity_String", "null channel");
        return 0;
    }
    pthread_mutex_lock(&ch->mutex);
    int32_t cap = (int32_t)ch->capacity;
    pthread_mutex_unlock(&ch->mutex);
    return cap;
}

bool pgy_channel_full_String(PgyChannel_String_RT *ch)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("full_String", "null channel");
        return false;
    }
    pthread_mutex_lock(&ch->mutex);
    bool full = ch->count >= ch->capacity;
    pthread_mutex_unlock(&ch->mutex);
    return full;
}

int32_t pgy_channel_space_String(PgyChannel_String_RT *ch)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("space_String", "null channel");
        return 0;
    }
    pthread_mutex_lock(&ch->mutex);
    int32_t space = (int32_t)(ch->capacity - ch->count);
    pthread_mutex_unlock(&ch->mutex);
    return space;
}

bool pgy_channel_closed_String(PgyChannel_String_RT *ch)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("closed_String", "null channel");
        return true;
    }
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
