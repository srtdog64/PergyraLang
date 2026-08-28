/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifndef _WIN32
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

#include "path_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "../common/string_compat.h"

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define PGY_ACCESS _access
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#define PGY_ACCESS access
#endif

char *
path_dirname_dup(const char *path)
{
    const char *last_sep;
    const char *last_bsep;

    if (path == NULL)
        return NULL;

    last_sep = strrchr(path, '/');
    last_bsep = strrchr(path, '\\');
    if (last_bsep != NULL && (last_sep == NULL || last_bsep > last_sep))
        last_sep = last_bsep;

    if (last_sep == NULL) {
        char *dot = malloc(2);
        if (dot) { dot[0] = '.'; dot[1] = '\0'; }
        return dot;
    }

    size_t len = (size_t)(last_sep - path);
    char *dir = malloc(len + 1);
    if (dir == NULL)
        return NULL;
    memcpy(dir, path, len);
    dir[len] = '\0';
    return dir;
}

char *
path_join_dup(const char *dir, const char *path)
{
    size_t dlen;
    size_t plen;
    size_t sep_len;
    bool needs_sep;
    char *result;

    if (dir == NULL || path == NULL)
        return NULL;

    dlen = strlen(dir);
    plen = strlen(path);
    needs_sep = dlen > 0 && dir[dlen - 1] != '/' && dir[dlen - 1] != '\\';
    sep_len = needs_sep ? 1u : 0u;
    if (dlen > SIZE_MAX - sep_len
        || dlen + sep_len > SIZE_MAX - plen
        || dlen + sep_len + plen > SIZE_MAX - 1) {
        return NULL;
    }
    result = malloc(dlen + sep_len + plen + 1);
    if (result == NULL)
        return NULL;

    memcpy(result, dir, dlen);
    if (needs_sep)
        result[dlen++] = '/';
    memcpy(result + dlen, path, plen);
    result[dlen + plen] = '\0';
    return result;
}

char *
path_replace_extension(const char *path, const char *new_ext)
{
    const char *dot;
    const char *last_sep;
    const char *last_bsep;
    size_t base_len;
    size_t ext_len;
    size_t new_len;
    char *result;

    if (path == NULL || new_ext == NULL)
        return NULL;

    dot = strrchr(path, '.');
    last_sep = strrchr(path, '/');
    last_bsep = strrchr(path, '\\');

    if (last_bsep != NULL && (last_sep == NULL || last_bsep > last_sep))
        last_sep = last_bsep;
    if (dot != NULL && last_sep != NULL && dot < last_sep)
        dot = NULL;

    base_len = dot ? (size_t)(dot - path) : strlen(path);
    ext_len = strlen(new_ext);
    if (ext_len > ((size_t)-1) - base_len - 1)
        return NULL;
    new_len = base_len + ext_len + 1;
    result = malloc(new_len);
    if (result == NULL)
        return NULL;

    memcpy(result, path, base_len);
    memcpy(result + base_len, new_ext, ext_len + 1);
    return result;
}

bool
path_file_exists(const char *path)
{
    return path != NULL && PGY_ACCESS(path, 0) == 0;
}

static bool
path_file_content_equals(const char *path, const char *expected_content)
{
    char *actual;
    bool matches;

    if (path == NULL || expected_content == NULL)
        return false;
    actual = path_read_file(path);
    if (actual == NULL)
        return false;
    matches = strcmp(actual, expected_content) == 0;
    free(actual);
    return matches;
}

#ifndef _WIN32
static bool
path_exchange_files_atomic(const char *left_path, const char *right_path)
{
#if defined(__linux__)
    return renameat2(AT_FDCWD, left_path, AT_FDCWD, right_path,
                     RENAME_EXCHANGE) == 0;
#elif defined(__APPLE__)
    return renamex_np(left_path, right_path, RENAME_SWAP) == 0;
#else
    (void)left_path;
    (void)right_path;
    return false;
#endif
}
#endif

#ifdef PGY_PATH_REPLACE_TEST_HOOKS
extern void pgy_path_replace_test_after_precheck(const char *dst_path);
extern bool pgy_path_replace_test_rollback_enabled(void);
#else
static void
pgy_path_replace_test_after_precheck(const char *dst_path)
{
    (void)dst_path;
}

static bool
pgy_path_replace_test_rollback_enabled(void)
{
    return true;
}
#endif

PathReplaceFileResult
path_replace_file_atomic_if_unchanged(const char *tmp_path,
                                      const char *dst_path,
                                      const char *backup_path,
                                      const char *expected_content)
{
    if (tmp_path == NULL || dst_path == NULL || expected_content == NULL)
        return PATH_REPLACE_ERROR;
    /* Do not expose formatted bytes merely to discover a conflict that was
     * already observable. A post-check remains necessary for the residual
     * race; that path is recoverable and its workspace must be preserved. */
    if (!path_file_content_equals(dst_path, expected_content))
        return PATH_REPLACE_SOURCE_CHANGED;
    pgy_path_replace_test_after_precheck(dst_path);
#ifdef _WIN32
    if (backup_path == NULL || backup_path[0] == '\0')
        return PATH_REPLACE_ERROR;
    if (!ReplaceFileA(dst_path, tmp_path, backup_path, 0, NULL, NULL))
        return PATH_REPLACE_ERROR;
    if (path_file_content_equals(backup_path, expected_content))
        return PATH_REPLACE_OK;
    if (pgy_path_replace_test_rollback_enabled())
        (void)ReplaceFileA(dst_path, backup_path, tmp_path, 0, NULL, NULL);
    return PATH_REPLACE_RECOVERY_REQUIRED;
#else
    struct stat destination;
    if (stat(dst_path, &destination) != 0)
        return PATH_REPLACE_ERROR;
    if (chmod(tmp_path, destination.st_mode & 07777) != 0)
        return PATH_REPLACE_ERROR;
    if (!path_exchange_files_atomic(tmp_path, dst_path))
        return PATH_REPLACE_ERROR;
    if (path_file_content_equals(tmp_path, expected_content))
        return PATH_REPLACE_OK;
    if (pgy_path_replace_test_rollback_enabled())
        (void)path_exchange_files_atomic(tmp_path, dst_path);
    return PATH_REPLACE_RECOVERY_REQUIRED;
#endif
}

#ifdef _WIN32
static bool
path_has_extension(const char *path)
{
    const char *dot = strrchr(path, '.');
    const char *last_sep = strrchr(path, '/');
    const char *last_bsep = strrchr(path, '\\');

    if (last_bsep != NULL && (last_sep == NULL || last_bsep > last_sep))
        last_sep = last_bsep;
    return dot != NULL && (last_sep == NULL || dot > last_sep);
}
#endif

char *
path_read_file(const char *path)
{
    if (path == NULL)
        return NULL;

    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }

    long sz = ftell(f);
    if (sz < 0 || (unsigned long)sz > (unsigned long)PGY_MAX_TEXT_FILE_BYTES) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    char *buf = malloc((size_t)sz + 1);
    if (buf == NULL) {
        fclose(f);
        return NULL;
    }

    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        free(buf);
        return NULL;
    }

    /* The lexer is NUL-terminated: an embedded NUL would silently truncate
     * everything after it (code present on disk, invisible to the compiler
     * — a review/supply-chain hazard, docs/189 C10).  Fail closed with an
     * observable cause instead. */
    const char *embedded_nul = memchr(buf, '\0', (size_t)sz);
    if (embedded_nul != NULL) {
        fprintf(stderr,
            "pgy: error: source file '%s' contains an embedded NUL byte at "
            "offset %ld; refusing to compile a partially visible file\n",
            path, (long)(embedded_nul - buf));
        fclose(f);
        free(buf);
        return NULL;
    }

    buf[sz] = '\0';
    fclose(f);
    return buf;
}

char *
path_default_binary(const char *source_path)
{
#ifdef _WIN32
    return path_replace_extension(source_path, ".exe");
#else
    return path_replace_extension(source_path, "");
#endif
}

char *
path_resolve_runnable_binary(const char *path)
{
    if (path == NULL)
        return NULL;

    if (path_file_exists(path))
        return pergyra_strdup(path);

#ifdef _WIN32
    if (!path_has_extension(path)) {
        size_t len = strlen(path);
        if (len > SIZE_MAX - 5)
            return NULL;
        char *with_ext = malloc(len + 5);
        if (with_ext == NULL)
            return NULL;
        memcpy(with_ext, path, len);
        memcpy(with_ext + len, ".exe", 5);
        if (path_file_exists(with_ext))
            return with_ext;
        free(with_ext);
    }
#endif

    return pergyra_strdup(path);
}
