/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "path_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "../common/string_compat.h"

#ifdef _WIN32
#include <io.h>
#define PGY_ACCESS _access
#else
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
