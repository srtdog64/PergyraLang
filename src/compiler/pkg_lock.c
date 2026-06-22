/*
 * Seashell deterministic package graph owner.
 */

#include "pkg_manifest.h"

#include "path_utils.h"

#include "../common/string_compat.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PGY_SEASHELL_LOCK_SCHEMA "pgy.seashell.lock.v1"

typedef struct
{
    char *schema;
    char *manifest_schema;
    char *name;
    char *version;
    char *source;
    char *entry;
    char *backend;
    bool deterministic;
    bool deterministic_set;
} PgyPackageLock;

static char *
lock_trim(char *text)
{
    char *end;

    if (text == NULL)
        return NULL;
    while (*text != '\0' && isspace((unsigned char)*text))
        text++;
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1]))
        *--end = '\0';
    return text;
}

static const char *
lock_trim_const(const char *text)
{
    while (text != NULL && *text != '\0' && isspace((unsigned char)*text))
        text++;
    return text;
}

static void
lock_strip_comment(char *line)
{
    bool in_string = false;

    if (line == NULL)
        return;
    for (char *p = line; *p != '\0'; p++) {
        if (*p == '"') {
            in_string = !in_string;
            continue;
        }
        if (*p == '#' && !in_string) {
            *p = '\0';
            return;
        }
    }
}

static bool
lock_split_key_value(char *line, char **key_out, char **value_out)
{
    char *eq;

    if (line == NULL || key_out == NULL || value_out == NULL)
        return false;
    eq = strchr(line, '=');
    if (eq == NULL)
        return false;
    *eq = '\0';
    *key_out = lock_trim(line);
    *value_out = lock_trim(eq + 1);
    return **key_out != '\0' && **value_out != '\0';
}

static char *
lock_parse_quoted_dup(const char *value)
{
    const char *start;
    const char *end;

    value = lock_trim_const(value);
    if (value == NULL || *value != '"')
        return NULL;
    start = value + 1;
    end = strchr(start, '"');
    if (end == NULL || *lock_trim_const(end + 1) != '\0')
        return NULL;
    return pergyra_strndup(start, (size_t)(end - start));
}

static bool
lock_assign_string(char **dst, const char *value)
{
    char *parsed = lock_parse_quoted_dup(value);

    if (parsed == NULL)
        return false;
    free(*dst);
    *dst = parsed;
    return true;
}

static bool
lock_parse_bool(const char *value, bool *out)
{
    value = lock_trim_const(value);
    if (value == NULL || out == NULL)
        return false;
    if (strcmp(value, "true") == 0) {
        *out = true;
        return true;
    }
    if (strcmp(value, "false") == 0) {
        *out = false;
        return true;
    }
    return false;
}

static bool
lock_parse_section(char *line, char *out, size_t out_size, bool *package_table)
{
    size_t len;
    bool array_table = false;

    line = lock_trim(line);
    len = strlen(line);
    if (len >= 4 && line[0] == '[' && line[1] == '['
        && line[len - 1] == ']' && line[len - 2] == ']') {
        array_table = true;
        if (len - 4 >= out_size)
            return false;
        memcpy(out, line + 2, len - 4);
        out[len - 4] = '\0';
    } else if (len >= 3 && line[0] == '[' && line[len - 1] == ']') {
        if (len - 2 >= out_size)
            return false;
        memcpy(out, line + 1, len - 2);
        out[len - 2] = '\0';
    } else {
        return false;
    }
    if (package_table != NULL)
        *package_table = array_table;
    return true;
}

static void
lock_destroy(PgyPackageLock *lock)
{
    if (lock == NULL)
        return;
    free(lock->schema);
    free(lock->manifest_schema);
    free(lock->name);
    free(lock->version);
    free(lock->source);
    free(lock->entry);
    free(lock->backend);
    memset(lock, 0, sizeof(*lock));
}

static bool
lock_load(PgyPackageLock *lock, const char *path)
{
    char *source;
    char *cursor;
    char section[64] = "";
    bool package_seen = false;
    unsigned line_no = 0;

    memset(lock, 0, sizeof(*lock));
    source = path_read_file(path);
    if (source == NULL)
        return false;
    cursor = source;
    while (cursor != NULL && *cursor != '\0') {
        char *line = cursor;
        char *next = strchr(cursor, '\n');
        char *key = NULL;
        char *value = NULL;
        bool package_table = false;

        line_no++;
        if (next != NULL) {
            *next = '\0';
            cursor = next + 1;
        } else {
            cursor = NULL;
        }
        if (line[0] != '\0' && line[strlen(line) - 1] == '\r')
            line[strlen(line) - 1] = '\0';
        lock_strip_comment(line);
        line = lock_trim(line);
        if (*line == '\0')
            continue;
        if (line[0] == '[') {
            if (!lock_parse_section(line, section, sizeof(section), &package_table)
                || (package_table && strcmp(section, "package") != 0)
                || (!package_table && strcmp(section, "seashell") != 0)) {
                fprintf(stderr, "pgy package: invalid pgy.lock section line %u\n",
                    line_no);
                free(source);
                return false;
            }
            if (package_table) {
                if (package_seen) {
                    fprintf(stderr,
                        "pgy package: pgy.lock contains multiple package entries\n");
                    free(source);
                    return false;
                }
                package_seen = true;
            }
            continue;
        }
        if (!lock_split_key_value(line, &key, &value)) {
            fprintf(stderr, "pgy package: invalid pgy.lock line %u\n", line_no);
            free(source);
            return false;
        }
        if (strcmp(section, "seashell") == 0) {
            if (strcmp(key, "schema") == 0) {
                if (!lock_assign_string(&lock->schema, value))
                    goto invalid;
            } else if (strcmp(key, "manifest_schema") == 0) {
                if (!lock_assign_string(&lock->manifest_schema, value))
                    goto invalid;
            } else {
                goto invalid;
            }
        } else if (strcmp(section, "package") == 0) {
            if (strcmp(key, "name") == 0) {
                if (!lock_assign_string(&lock->name, value))
                    goto invalid;
            } else if (strcmp(key, "version") == 0) {
                if (!lock_assign_string(&lock->version, value))
                    goto invalid;
            } else if (strcmp(key, "source") == 0) {
                if (!lock_assign_string(&lock->source, value))
                    goto invalid;
            } else if (strcmp(key, "entry") == 0) {
                if (!lock_assign_string(&lock->entry, value))
                    goto invalid;
            } else if (strcmp(key, "backend") == 0) {
                if (!lock_assign_string(&lock->backend, value))
                    goto invalid;
            } else if (strcmp(key, "deterministic") == 0) {
                if (!lock_parse_bool(value, &lock->deterministic))
                    goto invalid;
                lock->deterministic_set = true;
            } else {
                goto invalid;
            }
        } else {
            goto invalid;
        }
    }
    free(source);
    return package_seen && lock->schema != NULL && lock->manifest_schema != NULL
        && lock->name != NULL && lock->version != NULL && lock->source != NULL
        && lock->entry != NULL && lock->backend != NULL
        && lock->deterministic_set;

invalid:
    fprintf(stderr, "pgy package: invalid pgy.lock line %u\n", line_no);
    free(source);
    return false;
}

int
pgy_package_manifest_write_lock(const PgyPackageManifest *manifest)
{
    FILE *fp;

    if (manifest == NULL)
        return 1;
    fp = fopen("pgy.lock", "wb");
    if (fp == NULL) {
        fprintf(stderr, "pgy package: cannot write pgy.lock\n");
        return 1;
    }
    fprintf(fp,
        "# This file is generated by pgy package.\n"
        "# It records the deterministic package graph accepted by the beta package owner.\n"
        "\n"
        "[seashell]\n"
        "schema = \"%s\"\n"
        "manifest_schema = \"%s\"\n"
        "\n"
        "[[package]]\n"
        "name = \"%s\"\n"
        "version = \"%s\"\n"
        "source = \"path:.\"\n"
        "entry = \"%s\"\n"
        "backend = \"%s\"\n"
        "deterministic = %s\n",
        PGY_SEASHELL_LOCK_SCHEMA,
        manifest->seashell_schema,
        manifest->name,
        manifest->version,
        manifest->entry,
        pgy_package_manifest_backend_name(manifest->backend),
        manifest->deterministic ? "true" : "false");
    fclose(fp);
    printf("pgy package: wrote pgy.lock for '%s'\n", manifest->name);
    return 0;
}

bool
pgy_package_manifest_verify_existing_lock(
    const PgyPackageManifest *manifest,
    const char *path)
{
    PgyPackageLock lock;
    bool ok;

    if (manifest == NULL || path == NULL || !path_file_exists(path))
        return true;
    if (!lock_load(&lock, path)) {
        lock_destroy(&lock);
        fprintf(stderr, "pgy package: pgy.lock is not a valid Seashell lock\n");
        return false;
    }
    ok = strcmp(lock.schema, PGY_SEASHELL_LOCK_SCHEMA) == 0
        && strcmp(lock.manifest_schema, manifest->seashell_schema) == 0
        && strcmp(lock.name, manifest->name) == 0
        && strcmp(lock.version, manifest->version) == 0
        && strcmp(lock.source, "path:.") == 0
        && strcmp(lock.entry, manifest->entry) == 0
        && strcmp(lock.backend,
               pgy_package_manifest_backend_name(manifest->backend)) == 0
        && lock.deterministic == manifest->deterministic;
    lock_destroy(&lock);
    if (!ok) {
        fprintf(stderr,
            "pgy package: pgy.lock drift detected; run `pgy package` to refresh the deterministic package graph\n");
    }
    return ok;
}
