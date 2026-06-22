/*
 * Seashell manifest graph owner.
 *
 * This file owns the beta TOML-subset boundary for pgy.toml and pgy.lock.
 * Package command orchestration stays in pkg.c.
 */

#include "pkg_manifest.h"

#include "path_utils.h"

#include "../common/string_compat.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PGY_SEASHELL_SCHEMA "pgy.seashell.v1"
#define PGY_SEASHELL_FORMAT "toml-subset"

typedef enum
{
    PKG_SECTION_SEASHELL,
    PKG_SECTION_PACKAGE,
    PKG_SECTION_TARGETS_APP,
    PKG_SECTION_TARGETS_TEST,
    PKG_SECTION_DEPENDENCIES,
    PKG_SECTION_DEV_DEPENDENCIES,
    PKG_SECTION_EFFECTS,
    PKG_SECTION_AUTHORITY,
    PKG_SECTION_CAPABILITIES,
    PKG_SECTION_BUILD,
    PKG_SECTION_COUNT
} PgyManifestSection;

typedef struct
{
    bool section_seen[PKG_SECTION_COUNT];
    bool schema;
    bool format;
    bool name;
    bool version;
    bool pergyra;
    bool edition;
    bool app_main;
    bool test_main;
    bool effects_requires;
    bool authority_requires;
    bool capabilities_allow;
    bool capabilities_deny;
    bool backend;
    bool deterministic;
} PgyManifestSeen;

static char *
manifest_trim(char *text)
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
manifest_trim_const(const char *text)
{
    while (text != NULL && *text != '\0' && isspace((unsigned char)*text))
        text++;
    return text;
}

static bool
manifest_strip_comment(char *line)
{
    bool in_string = false;

    if (line == NULL)
        return false;
    for (char *p = line; *p != '\0'; p++) {
        if (*p == '\\' && in_string) {
            fprintf(stderr,
                "pgy package: Seashell TOML subset does not support escapes\n");
            return false;
        }
        if (*p == '"') {
            in_string = !in_string;
            continue;
        }
        if (*p == '#' && !in_string) {
            *p = '\0';
            return true;
        }
    }
    return true;
}

static bool
manifest_parse_section(char *line, char *out, size_t out_size)
{
    size_t len;

    if (line == NULL || out == NULL || out_size == 0)
        return false;
    line = manifest_trim(line);
    len = strlen(line);
    if (len < 3 || line[0] != '[' || line[len - 1] != ']')
        return false;
    if (line[1] == '[')
        return false;
    if (len - 2 >= out_size)
        return false;
    memcpy(out, line + 1, len - 2);
    out[len - 2] = '\0';
    return true;
}

static bool
manifest_split_key_value(char *line, char **key_out, char **value_out)
{
    char *eq;

    if (line == NULL || key_out == NULL || value_out == NULL)
        return false;
    eq = strchr(line, '=');
    if (eq == NULL)
        return false;
    *eq = '\0';
    *key_out = manifest_trim(line);
    *value_out = manifest_trim(eq + 1);
    return **key_out != '\0' && **value_out != '\0';
}

static bool
manifest_section_kind(const char *section, PgyManifestSection *out)
{
    static const struct {
        const char *name;
        PgyManifestSection kind;
    } sections[] = {
        {"authority", PKG_SECTION_AUTHORITY},
        {"build", PKG_SECTION_BUILD},
        {"capabilities", PKG_SECTION_CAPABILITIES},
        {"dependencies", PKG_SECTION_DEPENDENCIES},
        {"dev-dependencies", PKG_SECTION_DEV_DEPENDENCIES},
        {"effects", PKG_SECTION_EFFECTS},
        {"package", PKG_SECTION_PACKAGE},
        {"seashell", PKG_SECTION_SEASHELL},
        {"targets.app", PKG_SECTION_TARGETS_APP},
        {"targets.test", PKG_SECTION_TARGETS_TEST},
    };

    if (section == NULL || out == NULL)
        return false;
    for (size_t i = 0; i < sizeof(sections) / sizeof(sections[0]); i++) {
        if (strcmp(section, sections[i].name) == 0) {
            *out = sections[i].kind;
            return true;
        }
    }
    return false;
}

static bool
manifest_mark_key(bool *slot, const char *section, const char *key)
{
    if (slot == NULL || section == NULL || key == NULL)
        return false;
    if (*slot) {
        fprintf(stderr, "pgy package: duplicate Seashell key [%s].%s\n",
            section, key);
        return false;
    }
    *slot = true;
    return true;
}

static char *
manifest_parse_quoted_dup(const char *value)
{
    const char *start;
    const char *end;

    value = manifest_trim_const(value);
    if (value == NULL || *value != '"')
        return NULL;
    start = value + 1;
    end = strchr(start, '"');
    if (end == NULL)
        return NULL;
    if (*manifest_trim_const(end + 1) != '\0')
        return NULL;
    return pergyra_strndup(start, (size_t)(end - start));
}

static bool
manifest_assign_string(char **dst, const char *value)
{
    char *parsed = manifest_parse_quoted_dup(value);

    if (parsed == NULL)
        return false;
    free(*dst);
    *dst = parsed;
    return true;
}

static bool
manifest_parse_bool(const char *value, bool *out)
{
    value = manifest_trim_const(value);
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
manifest_parse_empty_array(const char *value)
{
    const char *p = manifest_trim_const(value);

    if (p == NULL || *p != '[')
        return false;
    p = manifest_trim_const(p + 1);
    if (*p != ']')
        return false;
    return *manifest_trim_const(p + 1) == '\0';
}

static bool
manifest_parse_backend(const char *value, BackendKind *backend)
{
    char *text;
    bool ok = false;

    value = manifest_trim_const(value);
    if (value == NULL || backend == NULL)
        return false;
    if (*value == '[') {
        fprintf(stderr,
            "pgy package: [build].backend must be a scalar string in Seashell v1\n");
        return false;
    }
    text = manifest_parse_quoted_dup(value);
    if (text == NULL)
        return false;
    if (strcmp(text, "c") == 0) {
        *backend = BACKEND_C;
        ok = true;
    } else if (strcmp(text, "llvm") == 0) {
        *backend = BACKEND_LLVM;
        ok = true;
    }
    free(text);
    return ok;
}

static bool
manifest_path_inside_package(const char *path)
{
    const char *p;
    const char *segment = path;

    if (path == NULL || *path == '\0')
        return false;
    if (path[0] == '/' || path[0] == '\\' || strchr(path, '\\') != NULL
        || strchr(path, ':') != NULL)
        return false;
    for (p = path;; p++) {
        if (*p == '/' || *p == '\0') {
            size_t len = (size_t)(p - segment);
            if (len == 0 || (len == 1 && segment[0] == '.')
                || (len == 2 && segment[0] == '.' && segment[1] == '.'))
                return false;
            if (*p == '\0')
                break;
            segment = p + 1;
        }
    }
    return true;
}

static bool
manifest_apply(PgyPackageManifest *manifest,
               PgyManifestSeen *seen,
               PgyManifestSection section,
               const char *section_name,
               const char *key,
               const char *value)
{
    bool parsed_bool;

    if (manifest == NULL || seen == NULL || key == NULL || value == NULL)
        return false;

    switch (section) {
    case PKG_SECTION_SEASHELL:
        if (strcmp(key, "schema") == 0)
            return manifest_mark_key(&seen->schema, section_name, key)
                && manifest_assign_string(&manifest->seashell_schema, value);
        if (strcmp(key, "format") == 0)
            return manifest_mark_key(&seen->format, section_name, key)
                && manifest_assign_string(&manifest->seashell_format, value);
        break;
    case PKG_SECTION_PACKAGE:
        if (strcmp(key, "name") == 0)
            return manifest_mark_key(&seen->name, section_name, key)
                && manifest_assign_string(&manifest->name, value);
        if (strcmp(key, "version") == 0)
            return manifest_mark_key(&seen->version, section_name, key)
                && manifest_assign_string(&manifest->version, value);
        if (strcmp(key, "pergyra") == 0)
            return manifest_mark_key(&seen->pergyra, section_name, key)
                && manifest_assign_string(&manifest->pergyra, value);
        if (strcmp(key, "edition") == 0)
            return manifest_mark_key(&seen->edition, section_name, key)
                && manifest_assign_string(&manifest->edition, value);
        break;
    case PKG_SECTION_TARGETS_APP:
        if (strcmp(key, "main") == 0)
            return manifest_mark_key(&seen->app_main, section_name, key)
                && manifest_assign_string(&manifest->entry, value);
        break;
    case PKG_SECTION_TARGETS_TEST:
        if (strcmp(key, "main") == 0)
            return manifest_mark_key(&seen->test_main, section_name, key)
                && manifest_assign_string(&manifest->test_entry, value);
        break;
    case PKG_SECTION_DEPENDENCIES:
    case PKG_SECTION_DEV_DEPENDENCIES:
        fprintf(stderr,
            "pgy package: dependency version solving is out-of-beta; remove dependency entries or vendor with file imports\n");
        return false;
    case PKG_SECTION_EFFECTS:
        if (strcmp(key, "requires") == 0) {
            if (!manifest_mark_key(&seen->effects_requires, section_name, key))
                return false;
            if (!manifest_parse_empty_array(value)) {
                fprintf(stderr,
                    "pgy package: non-empty effect declarations require an AIR verifier owner and are out-of-beta\n");
                return false;
            }
            return true;
        }
        break;
    case PKG_SECTION_AUTHORITY:
        if (strcmp(key, "requires") == 0) {
            if (!manifest_mark_key(&seen->authority_requires, section_name, key))
                return false;
            if (!manifest_parse_empty_array(value)) {
                fprintf(stderr,
                    "pgy package: non-empty authority declarations require an AIR verifier owner and are out-of-beta\n");
                return false;
            }
            return true;
        }
        break;
    case PKG_SECTION_CAPABILITIES:
        if (strcmp(key, "allow") == 0) {
            if (!manifest_mark_key(&seen->capabilities_allow, section_name, key))
                return false;
            if (!manifest_parse_empty_array(value)) {
                fprintf(stderr,
                    "pgy package: non-empty capability declarations require a capability verifier owner and are out-of-beta\n");
                return false;
            }
            return true;
        }
        if (strcmp(key, "deny") == 0) {
            if (!manifest_mark_key(&seen->capabilities_deny, section_name, key))
                return false;
            if (!manifest_parse_empty_array(value)) {
                fprintf(stderr,
                    "pgy package: non-empty capability declarations require a capability verifier owner and are out-of-beta\n");
                return false;
            }
            return true;
        }
        break;
    case PKG_SECTION_BUILD:
        if (strcmp(key, "backend") == 0) {
            if (!manifest_mark_key(&seen->backend, section_name, key)
                || !manifest_parse_backend(value, &manifest->backend))
                return false;
            manifest->backend_set = true;
            return true;
        }
        if (strcmp(key, "deterministic") == 0) {
            if (!manifest_mark_key(&seen->deterministic, section_name, key)
                || !manifest_parse_bool(value, &parsed_bool))
                return false;
            manifest->deterministic = parsed_bool;
            manifest->deterministic_set = true;
            return true;
        }
        break;
    case PKG_SECTION_COUNT:
        break;
    }

    fprintf(stderr, "pgy package: unsupported Seashell key [%s].%s\n",
        section_name, key);
    return false;
}

void
pgy_package_manifest_destroy(PgyPackageManifest *manifest)
{
    if (manifest == NULL)
        return;
    free(manifest->seashell_schema);
    free(manifest->seashell_format);
    free(manifest->name);
    free(manifest->version);
    free(manifest->pergyra);
    free(manifest->edition);
    free(manifest->entry);
    free(manifest->test_entry);
    memset(manifest, 0, sizeof(*manifest));
}

bool
pgy_package_manifest_load(PgyPackageManifest *manifest, const char *path)
{
    char *source;
    char *cursor;
    char section_name[96] = "";
    PgyManifestSection section = PKG_SECTION_COUNT;
    PgyManifestSeen seen;
    unsigned line_no = 0;

    if (manifest == NULL || path == NULL)
        return false;
    memset(manifest, 0, sizeof(*manifest));
    memset(&seen, 0, sizeof(seen));
    manifest->deterministic = true;

    source = path_read_file(path);
    if (source == NULL) {
        fprintf(stderr, "pgy package: cannot read %s\n", path);
        return false;
    }

    cursor = source;
    while (cursor != NULL && *cursor != '\0') {
        char *line = cursor;
        char *next = strchr(cursor, '\n');
        char *key = NULL;
        char *value = NULL;

        line_no++;
        if (next != NULL) {
            *next = '\0';
            cursor = next + 1;
        } else {
            cursor = NULL;
        }
        if (line[0] != '\0' && line[strlen(line) - 1] == '\r')
            line[strlen(line) - 1] = '\0';
        if (!manifest_strip_comment(line)) {
            fprintf(stderr, "pgy package: invalid pgy.toml line %u\n", line_no);
            free(source);
            return false;
        }
        line = manifest_trim(line);
        if (*line == '\0')
            continue;
        if (line[0] == '[') {
            if (!manifest_parse_section(line, section_name, sizeof(section_name))
                || !manifest_section_kind(section_name, &section)
                || seen.section_seen[section]) {
                fprintf(stderr,
                    "pgy package: unsupported or duplicate Seashell section on line %u\n",
                    line_no);
                free(source);
                return false;
            }
            seen.section_seen[section] = true;
            continue;
        }
        if (section == PKG_SECTION_COUNT
            || !manifest_split_key_value(line, &key, &value)
            || !manifest_apply(manifest, &seen, section, section_name, key, value)) {
            fprintf(stderr, "pgy package: invalid pgy.toml line %u\n", line_no);
            free(source);
            return false;
        }
    }
    free(source);

    if (manifest->seashell_schema == NULL
        || strcmp(manifest->seashell_schema, PGY_SEASHELL_SCHEMA) != 0) {
        fprintf(stderr,
            "pgy package: pgy.toml requires [seashell] schema = \"%s\"\n",
            PGY_SEASHELL_SCHEMA);
        return false;
    }
    if (manifest->seashell_format == NULL
        || strcmp(manifest->seashell_format, PGY_SEASHELL_FORMAT) != 0) {
        fprintf(stderr,
            "pgy package: pgy.toml requires [seashell] format = \"%s\"\n",
            PGY_SEASHELL_FORMAT);
        return false;
    }
    if (manifest->name == NULL || manifest->version == NULL
        || manifest->entry == NULL || !manifest->backend_set) {
        fprintf(stderr,
            "pgy package: pgy.toml requires package.name, package.version, targets.app.main, and build.backend\n");
        return false;
    }
    if (!pgy_package_manifest_package_name_is_valid(manifest->name)) {
        fprintf(stderr,
            "pgy package: package.name must use only letters, digits, '.', '_', or '-'\n");
        return false;
    }
    if (!manifest_path_inside_package(manifest->entry)
        || (manifest->test_entry != NULL
            && !manifest_path_inside_package(manifest->test_entry))) {
        fprintf(stderr,
            "pgy package: target entry paths must stay inside the package and use forward-slash relative paths\n");
        return false;
    }
    return true;
}

const char *
pgy_package_manifest_backend_name(BackendKind backend)
{
    return backend == BACKEND_LLVM ? "llvm" : "c";
}

bool
pgy_package_manifest_package_name_is_valid(const char *name)
{
    if (name == NULL || *name == '\0')
        return false;
    for (const unsigned char *p = (const unsigned char *)name; *p != '\0'; p++) {
        if (!(isalnum(*p) || *p == '.' || *p == '_' || *p == '-'))
            return false;
    }
    return true;
}

char *
pgy_package_manifest_entry_path_dup(
    const PgyPackageManifest *manifest,
    bool test_target)
{
    const char *entry;

    if (manifest == NULL)
        return NULL;
    entry = test_target && manifest->test_entry != NULL
        ? manifest->test_entry
        : manifest->entry;
    return pergyra_strdup(entry);
}

const char *
pgy_package_manifest_template(void)
{
    return
        "[seashell]\n"
        "schema = \"pgy.seashell.v1\"\nformat = \"toml-subset\"\n\n"
        "[package]\n"
        "name = \"%s\"\nversion = \"0.1.0\"\n"
        "pergyra = \"1.0\"\nedition = \"2026\"\n\n"
        "[targets.app]\n"
        "main = \"main.pgy\"\n\n"
        "[targets.test]\n"
        "main = \"main.pgy\"\n\n"
        "[dependencies]\n"
        "# Dependency version solving is out-of-beta; use file imports for now.\n\n"
        "[dev-dependencies]\n"
        "# test-utils = \"0.1.0\"\n\n"
        "[effects]\n"
        "requires = []\n\n"
        "[authority]\n"
        "requires = []\n\n"
        "[capabilities]\n"
        "allow = []\ndeny = []\n\n"
        "[build]\n"
        "backend = \"c\"\ndeterministic = true\n";
}
