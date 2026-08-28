#include "import_resolver_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "path_utils.h"

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define pgy_fullpath _fullpath
#else
#include <limits.h>
#include <unistd.h>
#endif

static bool
import_path_file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return false;
    fclose(f);
    return true;
}

#ifdef _WIN32
static char *
import_path_final_identity_dup(const char *path)
{
    HANDLE handle;
    DWORD required;
    DWORD written;
    char *resolved;

    handle = CreateFileA(path, 0,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (handle == INVALID_HANDLE_VALUE)
        return NULL;
    required = GetFinalPathNameByHandleA(
        handle, NULL, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (required == 0) {
        CloseHandle(handle);
        return NULL;
    }
    resolved = malloc((size_t)required + 1u);
    if (resolved == NULL) {
        CloseHandle(handle);
        return NULL;
    }
    written = GetFinalPathNameByHandleA(
        handle, resolved, required + 1u,
        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    CloseHandle(handle);
    if (written == 0 || written > required) {
        free(resolved);
        return NULL;
    }
    if (strncmp(resolved, "\\\\?\\UNC\\", 8) == 0) {
        size_t tail_length = strlen(resolved + 8);
        memmove(resolved + 2, resolved + 8, tail_length + 1);
        resolved[0] = '\\';
        resolved[1] = '\\';
    } else if (strncmp(resolved, "\\\\?\\", 4) == 0) {
        memmove(resolved, resolved + 4, strlen(resolved + 4) + 1);
    }
    return resolved;
}
#endif

static char *
path_parent_dir_dup(const char *path)
{
    char *dir = path_dirname_dup(path);
    char *parent;

    if (dir == NULL)
        return NULL;
    if (strcmp(dir, ".") == 0 || strcmp(dir, "/") == 0 || strcmp(dir, "\\") == 0)
        return dir;

    parent = path_dirname_dup(dir);
    free(dir);
    return parent;
}

char *
import_resolver_existing_final_identity_path_dup(const char *path)
{
    char *canonical = NULL;

    if (path == NULL)
        return NULL;

#ifdef _WIN32
    canonical = import_path_final_identity_dup(path);
#else
    {
        char *resolved = realpath(path, NULL);
        if (resolved != NULL)
            canonical = resolved;
    }
#endif
    if (canonical == NULL)
        return NULL;

#ifdef _WIN32
    /* Module identity is serialized into MIR and compared across native and
     * self-host producers. _fullpath() chooses '\\' while the Pergyra path
     * owner uses '/', so normalize the canonical spelling once at this owner
     * instead of teaching every downstream fact consumer both spellings. */
    for (char *p = canonical; *p != '\0'; ++p) {
        if (*p == '\\')
            *p = '/';
    }
#else
    if (strncmp(canonical, "/mnt/", 5) == 0
        && canonical[5] != '\0'
        && canonical[6] == '/') {
        for (char *p = canonical; *p != '\0'; ++p)
            *p = (char)tolower((unsigned char)*p);
    }
#endif

    return canonical;
}

char *
import_resolver_canonicalize_path_dup(const char *path)
{
    char *canonical;

    if (path == NULL)
        return NULL;
    canonical = import_resolver_existing_final_identity_path_dup(path);
    if (canonical != NULL)
        return canonical;
#ifdef _WIN32
    {
        char buffer[_MAX_PATH];
        if (pgy_fullpath(buffer, path, _MAX_PATH) != NULL)
            canonical = pergyra_strdup(buffer);
    }
#endif
    if (canonical == NULL)
        canonical = pergyra_strdup(path);
    return canonical;
}

char *
import_resolver_resolve_stdlib_module_path(const char *source_path,
                                           const char *module_name)
{
    char *search_dir = NULL;
    char *module_file = NULL;
    char *resolved = NULL;

    if (source_path == NULL || module_name == NULL)
        return NULL;

    search_dir = path_dirname_dup(source_path);
    module_file = malloc(strlen(module_name) + 5);
    if (search_dir == NULL || module_file == NULL)
        goto cleanup;

    snprintf(module_file, strlen(module_name) + 5, "%s.pgy", module_name);

    while (search_dir != NULL) {
        char *stdlib_dir = path_join_dup(search_dir, "stdlib");
        char *candidate = stdlib_dir != NULL ? path_join_dup(stdlib_dir, module_file) : NULL;
        char *parent = NULL;

        free(stdlib_dir);

        if (candidate != NULL && import_path_file_exists(candidate)) {
            resolved = candidate;
            break;
        }
        free(candidate);

        parent = path_parent_dir_dup(search_dir);
        if (parent == NULL || strcmp(parent, search_dir) == 0) {
            free(parent);
            break;
        }
        free(search_dir);
        search_dir = parent;
    }

    if (resolved == NULL) {
        char *candidate = path_join_dup("stdlib", module_file);
        if (candidate != NULL && import_path_file_exists(candidate))
            resolved = candidate;
        else
            free(candidate);
    }

cleanup:
    free(search_dir);
    free(module_file);
    return resolved;
}
