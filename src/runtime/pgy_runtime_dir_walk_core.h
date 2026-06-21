#ifndef PGY_RUNTIME_DIR_WALK_CORE_H
#define PGY_RUNTIME_DIR_WALK_CORE_H

#ifndef PGY_RUNTIME_DIR_WALK_PUBLIC
#error "PGY_RUNTIME_DIR_WALK_PUBLIC must be defined before including dir walk core"
#endif

#ifndef PGY_RUNTIME_DIR_WALK_STRDUP
#error "PGY_RUNTIME_DIR_WALK_STRDUP must be defined before including dir walk core"
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
extern int lstat(const char *path, struct stat *buf);
#endif

static char *
pgy_dir_walk_display_root_dup(const char *root)
{
    size_t len;
    char *copy;

    if (root == NULL || root[0] == '\0')
        return NULL;
    copy = PGY_RUNTIME_DIR_WALK_STRDUP(root);
    if (copy == NULL)
        return NULL;
    for (char *p = copy; *p != '\0'; p++) {
        if (*p == '\\')
            *p = '/';
    }
    len = strlen(copy);
    while (len > 1 && copy[len - 1] == '/') {
        copy[len - 1] = '\0';
        len--;
    }
    if (strcmp(copy, ".") == 0) {
        copy[0] = '\0';
    }
    return copy;
}

static char *
pgy_dir_walk_join_display_dup(const char *dir, const char *name)
{
    size_t dlen;
    size_t nlen;
    char *out;

    if (name == NULL)
        return NULL;
    if (dir == NULL || dir[0] == '\0')
        return PGY_RUNTIME_DIR_WALK_STRDUP(name);

    dlen = strlen(dir);
    nlen = strlen(name);
    out = (char *)malloc(dlen + 1 + nlen + 1);
    if (out == NULL)
        return NULL;
    memcpy(out, dir, dlen);
    out[dlen] = '/';
    memcpy(out + dlen + 1, name, nlen + 1);
    return out;
}

static void
pgy_dir_walk_push_file(PgyArray_String *out, const char *display_path)
{
    char *owned;

    if (out == NULL || display_path == NULL)
        return;
    owned = PGY_RUNTIME_DIR_WALK_STRDUP(display_path);
    if (owned == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
    }
    pgy_array_push_String(out, owned);
}

static bool
pgy_dir_walk_skip_name(const char *name)
{
    return name == NULL
        || strcmp(name, ".") == 0
        || strcmp(name, "..") == 0;
}

#ifdef _WIN32
static void
pgy_dir_walk_collect(PgyArray_String *out,
                     const char *resolved_dir,
                     const char *display_dir)
{
    char *pattern;
    WIN32_FIND_DATAA data;
    HANDLE handle;

    if (out == NULL || resolved_dir == NULL || display_dir == NULL)
        return;

    pattern = pgy_runtime_path_join_dup(resolved_dir, "*");
    if (pattern == NULL)
        return;

    handle = FindFirstFileA(pattern, &data);
    free(pattern);
    if (handle == INVALID_HANDLE_VALUE)
        return;

    do {
        char *resolved_child;
        char *display_child;

        if (pgy_dir_walk_skip_name(data.cFileName))
            continue;
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
            continue;

        resolved_child = pgy_runtime_path_join_dup(resolved_dir, data.cFileName);
        display_child = pgy_dir_walk_join_display_dup(display_dir, data.cFileName);
        if (resolved_child == NULL || display_child == NULL) {
            free(resolved_child);
            free(display_child);
            continue;
        }

        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            pgy_dir_walk_collect(out, resolved_child, display_child);
        } else {
            pgy_dir_walk_push_file(out, display_child);
        }

        free(resolved_child);
        free(display_child);
    } while (FindNextFileA(handle, &data));

    FindClose(handle);
}
#else
static void
pgy_dir_walk_collect(PgyArray_String *out,
                     const char *resolved_dir,
                     const char *display_dir)
{
    DIR *dir;
    struct dirent *entry;

    if (out == NULL || resolved_dir == NULL || display_dir == NULL)
        return;

    dir = opendir(resolved_dir);
    if (dir == NULL)
        return;

    while ((entry = readdir(dir)) != NULL) {
        char *resolved_child;
        char *display_child;
        struct stat st;

        if (pgy_dir_walk_skip_name(entry->d_name))
            continue;

        resolved_child = pgy_runtime_path_join_dup(resolved_dir, entry->d_name);
        display_child = pgy_dir_walk_join_display_dup(display_dir, entry->d_name);
        if (resolved_child == NULL || display_child == NULL) {
            free(resolved_child);
            free(display_child);
            continue;
        }

        if (lstat(resolved_child, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                pgy_dir_walk_collect(out, resolved_child, display_child);
            } else if (S_ISREG(st.st_mode)) {
                pgy_dir_walk_push_file(out, display_child);
            }
        }

        free(resolved_child);
        free(display_child);
    }

    closedir(dir);
}
#endif

PGY_RUNTIME_DIR_WALK_PUBLIC PgyArray_String
pgy_dir_walk(const char *root)
{
    PgyArray_String out;
    char *resolved;
    char *display_root;

    pgy_cap_require_export(PGY_CAP_IO_READ, "dir-walk");
    out = pgy_array_new_String(8);
    display_root = pgy_dir_walk_display_root_dup(root);
    resolved = pgy_runtime_resolve_file_path(root, false);
    if (display_root == NULL || resolved == NULL) {
        free(display_root);
        free(resolved);
        return out;
    }

    pgy_dir_walk_collect(&out, resolved, display_root);
    pgy_array_sort_String(out.data, out.length);

    free(display_root);
    free(resolved);
    return out;
}

#endif /* PGY_RUNTIME_DIR_WALK_CORE_H */
