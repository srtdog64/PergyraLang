/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * compiler_runtime_cache.c -- build/cache of the separately-compiled runtime
 * object (pgy_runtime_lib.o).
 *
 * The object holds the runtime's external-linkage bodies. Two consumers link
 * it:
 *   - the LLVM leg, whose emitted IR calls the runtime by symbol; and
 *   - the C leg in extern mode (PGY_RUNTIME_DECLS_ONLY), where emitted C parses
 *     only prototypes and links the bodies here instead of re-inlining ~9k
 *     lines of runtime per translation unit (docs/189 C14 / WO-RED2).
 *
 * Both legs share one cache path and one build recipe (compiler_runtime_object
 * _ensure) so the linked runtime cannot drift between backends. This file is no
 * longer gated on PGY_LLVM_ENABLED: the C leg needs the object even in builds
 * with LLVM disabled.
 */

#include "compiler_toolchain.h"
#include "compiler_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <dirent.h>

#ifdef _WIN32
#include <sys/stat.h>
#include <process.h>
#define PGY_STAT _stat
#define PGY_STAT_STRUCT struct _stat
#define PGY_GETPID _getpid
#else
#include <sys/stat.h>
#include <unistd.h>
#define PGY_STAT stat
#define PGY_STAT_STRUCT struct stat
#define PGY_GETPID getpid
#endif

#include "../common/string_compat.h"

static const char *
compiler_temp_dir(void)
{
    const char *tmpdir = getenv("TMPDIR");

    if (tmpdir == NULL || tmpdir[0] == '\0')
        tmpdir = getenv("TMP");
    if (tmpdir == NULL || tmpdir[0] == '\0')
        tmpdir = getenv("TEMP");
#ifdef _WIN32
    if (tmpdir == NULL || tmpdir[0] == '\0')
        tmpdir = ".";
#else
    if (tmpdir == NULL || tmpdir[0] == '\0')
        tmpdir = "/tmp";
#endif
    return tmpdir;
}

static bool
compiler_file_mtime(const char *path, time_t *mtime_out)
{
    PGY_STAT_STRUCT st;

    if (path == NULL || mtime_out == NULL)
        return false;
    if (PGY_STAT(path, &st) != 0)
        return false;
    *mtime_out = st.st_mtime;
    return true;
}

/*
 * Freshness used to be gated on a hand-maintained header list; every runtime
 * header added after the list was written became a silent blind spot -- edit
 * it and a stale object still passed as "fresh", linking old semantics into the
 * emitted binary (the same failure mode as the .bc list, docs/189 C6). Fail
 * closed instead: any .h/.c under the runtime directory newer than the object
 * makes it stale, and an unreadable directory refuses freshness outright.
 *
 * Three further blind spots are closed here (docs/190 B6/B7):
 *   - the scan RECURSES; the flat readdir never saw src/runtime/async/, whose
 *     scheduler/fiber sources the runtime TU includes, so editing them left the
 *     object "fresh";
 *   - each directory's own mtime is a dependency, which is what catches a
 *     DELETED source: removing a file leaves no surviving file newer than the
 *     object, but it does bump the containing directory's mtime;
 *   - the comparison is `<=`, not `<`. st_mtime has 1-second granularity, so an
 *     edit landing in the same second the object was written otherwise reads as
 *     fresh -- a wide window when sources and the object sit on different
 *     filesystems (E:\ source tree vs C:\...\Temp object). At worst `<=` costs
 *     one redundant rebuild; `<` costs a silently stale link.
 * The depth cap is a cheap guard against a symlinked cycle under the tree.
 */
#define PGY_RUNTIME_SCAN_MAX_DEPTH 8

static bool
compiler_runtime_dir_has_newer_source_at(const char *dir_path, time_t obj_mtime,
                                         int depth)
{
    DIR *dir;
    struct dirent *entry;
    time_t dir_mtime;

    if (depth > PGY_RUNTIME_SCAN_MAX_DEPTH)
        return true; /* fail closed: refuse to prove freshness past the cap */

    /* A create/delete/rename in this directory bumps its own mtime. */
    if (!compiler_file_mtime(dir_path, &dir_mtime) || obj_mtime <= dir_mtime)
        return true;

    dir = opendir(dir_path);
    if (dir == NULL)
        return true; /* fail closed: cannot prove freshness */

    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        char path[1024];
        int written;
        PGY_STAT_STRUCT st;

        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;

        written = snprintf(path, sizeof(path), "%s/%s", dir_path, name);
        if (written < 0 || (size_t)written >= sizeof(path)) {
            closedir(dir);
            return true; /* fail closed on unrepresentable path */
        }
        if (PGY_STAT(path, &st) != 0) {
            closedir(dir);
            return true; /* fail closed: cannot stat a tree entry */
        }

        if ((st.st_mode & S_IFMT) == S_IFDIR) {
            if (compiler_runtime_dir_has_newer_source_at(path, obj_mtime,
                                                         depth + 1)) {
                closedir(dir);
                return true;
            }
            continue;
        }

        if (!(len > 2 && (strcmp(name + len - 2, ".h") == 0
                          || strcmp(name + len - 2, ".c") == 0)))
            continue;

        if (obj_mtime <= st.st_mtime) {
            closedir(dir);
            return true;
        }
    }
    closedir(dir);
    return false;
}

static bool
compiler_runtime_dir_has_newer_source(const char *dir_path, time_t obj_mtime)
{
    return compiler_runtime_dir_has_newer_source_at(dir_path, obj_mtime, 0);
}

bool
compiler_runtime_cache_is_fresh(const char *cache_obj_path)
{
    time_t cache_mtime;
    /* Sources the runtime TU includes from outside the runtime directory; the
     * runtime directory itself is covered by the dir scan below (which sees
     * pgy_runtime_lib.c and every *_inline.h / *_exports.* automatically). */
    const char *extra_deps[] = {
        PGY_SRC_DIR "/common/string_compat.h",
        PGY_SRC_DIR "/common/execution_lane_kind.h",
        NULL
    };

    if (!compiler_file_mtime(cache_obj_path, &cache_mtime))
        return false;
    for (size_t i = 0; extra_deps[i] != NULL; i++) {
        time_t dep_mtime;

        if (!compiler_file_mtime(extra_deps[i], &dep_mtime))
            return false;
        if (cache_mtime <= dep_mtime)   /* `<=`: same-second edits (docs/190 B7) */
            return false;
    }
    if (compiler_runtime_dir_has_newer_source(PGY_RUNTIME_DIR, cache_mtime))
        return false;
    return true;
}

char *
compiler_runtime_prebuilt_object_path(PgyOptProfile opt_profile,
                                      bool uses_intent_observability)
{
    char key[64];
    const char *opt_name = (opt_profile == PGY_OPT_RELEASE) ? "RELEASE" : "DEV";
    const char *obs_name = uses_intent_observability ? "OBS1" : "OBS0";
    const char *value;

    snprintf(key, sizeof(key), "PGY_PREBUILT_RUNTIME_OBJ_%s_%s", opt_name, obs_name);
    value = getenv(key);
    if (value == NULL || value[0] == '\0')
        value = getenv("PGY_PREBUILT_RUNTIME_OBJ");
    if (value == NULL || value[0] == '\0')
        return NULL;
    return pergyra_strdup(value);
}

char *
compiler_runtime_cache_object_path(PgyOptProfile opt_profile,
                                   bool uses_intent_observability)
{
    const char *tmpdir = compiler_temp_dir();
    const char *opt_name = (opt_profile == PGY_OPT_RELEASE) ? "release" : "dev";
    const char *obs_name = uses_intent_observability ? "obs1" : "obs0";
    char buf[1024];
#ifdef _WIN32
    const char *ext = ".obj";
#else
    const char *ext = ".o";
#endif

    snprintf(buf, sizeof(buf), "%s/pgy_runtime_cache_%s_%s%s",
             tmpdir, opt_name, obs_name, ext);
    return pergyra_strdup(buf);
}

/* Cache path for the C-leg extern object (pgy_runtime_cext_lib.c). Distinct
 * from the LLVM leg's exports object above so the two never clobber each other
 * in a shared temp dir. */
static char *
compiler_cext_object_path(PgyOptProfile opt_profile,
                          bool uses_intent_observability)
{
    const char *tmpdir = compiler_temp_dir();
    const char *opt_name = (opt_profile == PGY_OPT_RELEASE) ? "release" : "dev";
    const char *obs_name = uses_intent_observability ? "obs1" : "obs0";
    char buf[1024];
#ifdef _WIN32
    const char *ext = ".obj";
#else
    const char *ext = ".o";
#endif

    snprintf(buf, sizeof(buf), "%s/pgy_runtime_cext_%s_%s%s",
             tmpdir, opt_name, obs_name, ext);
    return pergyra_strdup(buf);
}

/*
 * Ensure a fresh runtime object exists for (opt_profile, observability) and
 * return its path (caller frees). The single build recipe both backends share.
 *
 *   - A user-supplied prebuilt object (PGY_PREBUILT_RUNTIME_OBJ*) is used only
 *     if it is fresh; a stale prebuilt is an error, never a silent rebuild over
 *     the user's artifact.
 *   - Otherwise the per-profile cache object is rebuilt when the dir scan says
 *     it is stale, and reused when fresh.
 *
 * Returns NULL on failure with *error_out set to a static message.
 */
char *
compiler_runtime_object_ensure(PgyOptProfile opt_profile,
                               bool uses_intent_observability,
                               bool verbose,
                               const char **error_out)
{
    const char *dummy = NULL;
    if (error_out == NULL)
        error_out = &dummy;
    *error_out = NULL;

    char *obj_path = compiler_cext_object_path(
        opt_profile, uses_intent_observability);
    if (obj_path == NULL) {
        *error_out = "Out of memory resolving runtime object path";
        return NULL;
    }
    if (!pgy_path_is_safe(obj_path)) {
        free(obj_path);
        *error_out = "Unsafe characters in runtime object path";
        return NULL;
    }

    if (compiler_runtime_cache_is_fresh(obj_path))
        return obj_path; /* cache hit */

    PgyCCompilerSelection cc_selection;
    if (!pgy_select_c_compiler(&cc_selection)) {
        free(obj_path);
        *error_out = "Unable to detect C compiler for runtime object";
        return NULL;
    }
    const char *cc = cc_selection.cc;
    const char *cc_target = cc_selection.target_flag;
    const char *opt_flag = (opt_profile == PGY_OPT_RELEASE) ? "-O3" : "-O0";
    const char *obs_flag = uses_intent_observability
        ? "-DPGY_INTENT_OBSERVABILITY_ENABLED=1"
        : "-DPGY_INTENT_OBSERVABILITY_ENABLED=0";

    /* Per-process scratch path; the finished object is renamed into the cache
     * below so no other process ever observes a partial write. */
    char tmp_buf[1024];
    char *tmp_path;
    int tmp_written = snprintf(tmp_buf, sizeof(tmp_buf), "%s.tmp%ld",
                               obj_path, (long)PGY_GETPID());
    if (tmp_written < 0 || (size_t)tmp_written >= sizeof(tmp_buf)) {
        free(obj_path);
        *error_out = "Runtime object scratch path too long";
        return NULL;
    }
    tmp_path = pergyra_strdup(tmp_buf);
    if (tmp_path == NULL) {
        free(obj_path);
        *error_out = "Out of memory resolving runtime object scratch path";
        return NULL;
    }

    /*
     * Same arithmetic/aliasing flags as the emitted-C compile (-fwrapv /
     * -fno-strict-aliasing keep checked arithmetic UB-free under -O3), so the
     * linked runtime body behaves identically to the inline body it replaces.
     * pgy_runtime_cext_lib.c owns PGY_RUNTIME_EXTERN_DEFS, making PGY_RT_DECL
     * expand to an external-linkage definition and turning the *_inline.h
     * bodies into the one linked runtime.
     */
    const char *argv[24];
    int argc = 0;
    argv[argc++] = cc;
    if (cc_target != NULL) argv[argc++] = cc_target;
    argv[argc++] = "-std=c11";
    argv[argc++] = "-Wall";
#ifdef _WIN32
    argv[argc++] = "-Wno-unused-value";
    argv[argc++] = "-Wno-parentheses-equality";
    argv[argc++] = "-Wno-c23-extensions";
    argv[argc++] = "-Wno-format-truncation";
    argv[argc++] = PGY_CFLAGS_THREAD_FLAG;
#elif defined(__APPLE__)
    argv[argc++] = "-D_DARWIN_C_SOURCE";
    argv[argc++] = "-D_XOPEN_SOURCE=700";
#else
    argv[argc++] = "-D_POSIX_C_SOURCE=200809L";
    argv[argc++] = "-D_XOPEN_SOURCE=700";
#endif
    argv[argc++] = opt_flag;
    argv[argc++] = "-fwrapv";
    argv[argc++] = "-fno-strict-aliasing";
#if !defined(_WIN32) && !defined(__APPLE__)
    argv[argc++] = "-fopenmp";
#endif
    argv[argc++] = obs_flag;
    argv[argc++] = "-I";
    argv[argc++] = PGY_SRC_DIR;
    argv[argc++] = "-c";
    argv[argc++] = PGY_RUNTIME_CEXT_LIB_C;
    argv[argc++] = "-o";
    argv[argc++] = tmp_path;
    argv[argc] = NULL;

    if (pgy_exec_argv(argv, verbose) != 0) {
        remove(tmp_path);
        free(tmp_path);
        free(obj_path);
        *error_out = "Runtime object compilation failed";
        return NULL;
    }

    /* Publish atomically (docs/190 B2). Writing straight to the cache path let
     * a concurrent builder -- the local compare harness defaults to 8 jobs --
     * or a killed cc leave a torn object behind, and because a partial write
     * still carries a NEWER mtime than every source, the freshness check would
     * then call that garbage "fresh" forever. rename replaces atomically on
     * POSIX; Windows cannot replace an existing target, so unlink first, which
     * is still far tighter than leaving the real path truncated for the whole
     * compile. Two racing builders now each publish a complete object. */
#ifdef _WIN32
    remove(obj_path);
#endif
    if (rename(tmp_path, obj_path) != 0) {
        remove(tmp_path);
        free(tmp_path);
        free(obj_path);
        *error_out = "Runtime object could not be published into the cache";
        return NULL;
    }
    free(tmp_path);
    return obj_path;
}
