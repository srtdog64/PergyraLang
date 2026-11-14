#ifdef PGY_LLVM_ENABLED

#include "llvm_runtime_bitcode_freshness.h"

#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <sys/stat.h>
#define PGY_LLVM_STAT _stat
#define PGY_LLVM_STAT_STRUCT struct _stat
#else
#include <sys/stat.h>
#define PGY_LLVM_STAT stat
#define PGY_LLVM_STAT_STRUCT struct stat
#endif

#ifndef PGY_RUNTIME_DIR
#define PGY_RUNTIME_DIR "src/runtime"
#endif

#ifndef PGY_RUNTIME_LIB_C
#define PGY_RUNTIME_LIB_C "src/runtime/pgy_runtime_lib.c"
#endif

static bool
llvm_runtime_file_mtime(const char *path, time_t *mtime_out)
{
    PGY_LLVM_STAT_STRUCT st;

    if (path == NULL || mtime_out == NULL)
        return false;
    if (PGY_LLVM_STAT(path, &st) != 0)
        return false;
    *mtime_out = st.st_mtime;
    return true;
}

/* Freshness used to be gated on a hand-maintained header list; every header
 * added to the runtime after the list was written became a silent blind spot
 * (edit it and a stale .bc still passed as "fresh", inlining old semantics
 * into the LLVM leg only — docs/189 C6).  Fail closed instead: any .h/.c
 * source in the runtime directory newer than the bitcode makes it stale; an
 * unreadable directory refuses freshness outright. */
static bool
llvm_runtime_dir_has_newer_source(const char *dir_path, time_t bc_mtime)
{
    DIR *dir = opendir(dir_path);
    if (dir == NULL)
        return true; /* fail closed: cannot prove freshness */

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        bool is_source = (len > 2 && strcmp(name + len - 2, ".h") == 0)
            || (len > 2 && strcmp(name + len - 2, ".c") == 0);
        if (!is_source)
            continue;

        char path[1024];
        int written = snprintf(path, sizeof(path), "%s/%s", dir_path, name);
        if (written < 0 || (size_t)written >= sizeof(path)) {
            closedir(dir);
            return true; /* fail closed on unrepresentable path */
        }

        time_t dep_mtime;
        if (!llvm_runtime_file_mtime(path, &dep_mtime)
            || bc_mtime < dep_mtime) {
            closedir(dir);
            return true;
        }
    }
    closedir(dir);
    return false;
}

bool
llvm_runtime_bitcode_is_fresh(const char *bc_path)
{
    time_t bc_mtime;
    /* Sources living outside the runtime directory that the runtime TU
     * still includes; keep this list to out-of-directory files only. */
    const char *extra_deps[] = {
        PGY_RUNTIME_LIB_C,
        PGY_RUNTIME_DIR "/../common/execution_lane_kind.h",
        NULL
    };

    if (!llvm_runtime_file_mtime(bc_path, &bc_mtime))
        return false;
    for (size_t i = 0; extra_deps[i] != NULL; i++) {
        time_t dep_mtime;
        if (!llvm_runtime_file_mtime(extra_deps[i], &dep_mtime))
            return false;
        if (bc_mtime < dep_mtime)
            return false;
    }
    if (llvm_runtime_dir_has_newer_source(PGY_RUNTIME_DIR, bc_mtime))
        return false;
    return true;
}

#endif /* PGY_LLVM_ENABLED */
