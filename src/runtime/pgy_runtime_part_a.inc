/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * pgy_runtime.h — Hybrid memory model runtime for Pergyra
 *
 * Memory Model:
 *   - Stack: Default for value types (zero overhead)
 *   - Heap: Box<T>, Arena for dynamic allocation
 *   - Slot: Optional safety wrapper with debug checks
 *   - Unsafe: Raw pointers for FFI (inside unsafe { } blocks)
 *
 * Build Modes:
 *   - Debug: PGY_DEBUG defined — safety checks enabled
 *   - Release: PGY_DEBUG not defined — zero-overhead
 */

#ifndef PGY_RUNTIME_H
#define PGY_RUNTIME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <pthread.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif
#include "pgy_parallel.h"

#ifndef PGY_RUNTIME_MAX_FILE_BYTES
#define PGY_RUNTIME_MAX_FILE_BYTES (64u * 1024u * 1024u)
#endif

static inline char *pgy_runtime_strdup(const char *src);

static inline void
pgy_runtime_warn_intent_enter_failure(const char *name, const char *reason,
                                      int32_t priority, bool is_concurrent)
{
    fprintf(stderr, "[pgy][intent] enter %s failed: %s (priority=%d concurrent=%s)\n",
            name != NULL ? name : "<intent>",
            reason != NULL ? reason : "unknown reason",
            (int)priority,
            is_concurrent ? "true" : "false");
}

static inline void
pgy_runtime_warn_invalid_intent_index(const char *op, int32_t index, int32_t count)
{
    fprintf(stderr, "[pgy][intent] %s: invalid index %d (count=%d)\n",
            op != NULL ? op : "<op>", (int)index, (int)count);
}

static inline void
pgy_runtime_warn_invalid_channel(const char *op, const char *reason)
{
    fprintf(stderr, "[pgy][channel] %s: %s\n",
            op != NULL ? op : "<op>",
            reason != NULL ? reason : "invalid channel operation");
}

static inline void
pgy_runtime_warn_invalid_collection(const char *op, const char *reason)
{
    fprintf(stderr, "[pgy][collection] %s: %s\n",
            op != NULL ? op : "<op>",
            reason != NULL ? reason : "invalid collection operation");
}

static inline size_t
pgy_runtime_channel_capacity_or_default(const char *op, size_t cap)
{
    if (cap == 0) {
        pgy_runtime_warn_invalid_channel(op, "zero capacity requested; using capacity=1");
        return 1;
    }
    return cap;
}

static inline bool
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

static inline bool
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

static inline bool
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

static inline bool
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

static inline char *
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
static inline void
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

static inline char *
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
        return pgy_runtime_strdup(".");

    len = (size_t)(last_sep - path);
    dir = (char *)malloc(len + 1);
    if (dir == NULL)
        return NULL;
    memcpy(dir, path, len);
    dir[len] = '\0';
    return dir;
}

static inline bool
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

static inline char *
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
    return pgy_runtime_strdup(resolved);
#endif
}

#ifdef _WIN32
static inline bool
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

static inline char *
pgy_runtime_resolve_file_path(const char *path, bool for_write)
{
    const char *root = getenv("PGY_IO_ROOT");
    const char *allow_abs;
    char *candidate = NULL;

    if (!pgy_runtime_file_path_allowed(path))
        return NULL;

    if (root != NULL && root[0] != '\0') {
        if (pgy_runtime_path_is_absolute(path))
            candidate = pgy_runtime_strdup(path);
        else
            candidate = pgy_runtime_path_join_dup(root, path);
    } else if (pgy_runtime_path_is_absolute(path)) {
        allow_abs = getenv("PGY_IO_ALLOW_ABSOLUTE");
        if (allow_abs == NULL || strcmp(allow_abs, "1") != 0)
            return NULL;
        candidate = pgy_runtime_strdup(path);
    } else {
        candidate = pgy_runtime_strdup(path);
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
            check_path = pgy_runtime_strdup(candidate);

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
            check_path = pgy_runtime_strdup(candidate);

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

