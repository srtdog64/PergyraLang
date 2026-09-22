#include "driver_binary_output_owner.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "../common/string_compat.h"
#include "compiler_toolchain.h"
#include "driver_diag.h"
#include "driver_self_host_selection_owner.h"
#include "path_utils.h"

#ifdef _WIN32
static bool
driver_binary_output_object_suffix(const char *path)
{
    size_t length = strlen(path);
    return (length >= 2 && strcmp(path + length - 2, ".o") == 0)
        || (length >= 4 && strcmp(path + length - 4, ".obj") == 0);
}
#endif

char *
driver_binary_output_resolve(const DriverFlags *flags)
{
    char *requested;
    if (flags == NULL || flags->source_path == NULL)
        return NULL;
    requested = flags->output_path != NULL
        ? pergyra_strdup(flags->output_path)
        : path_default_binary(flags->source_path);
    if (requested == NULL)
        return NULL;
#ifdef _WIN32
    if (flags->backend == BACKEND_LLVM && flags->do_run
        && driver_binary_output_object_suffix(requested)) {
        char *runnable = path_replace_extension(requested, ".exe");
        free(requested);
        return runnable;
    }
#endif
    return requested;
}

static bool
driver_binary_output_reject(const DriverFlags *flags, const char *reason)
{
    driver_emit_stage_fail(flags, "binary_output",
        "binary output invalidation failed", reason);
    return false;
}

static bool
driver_binary_output_is_source(const char *source, const char *output)
{
    if (strcmp(source, output) == 0)
        return true;
#ifdef _WIN32
    HANDLE source_file = CreateFileA(source, FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE output_file = CreateFileA(output, FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    BY_HANDLE_FILE_INFORMATION source_info;
    BY_HANDLE_FILE_INFORMATION output_info;
    bool same = false;
    if (source_file != INVALID_HANDLE_VALUE && output_file != INVALID_HANDLE_VALUE
        && GetFileInformationByHandle(source_file, &source_info)
        && GetFileInformationByHandle(output_file, &output_info)) {
        same = source_info.dwVolumeSerialNumber == output_info.dwVolumeSerialNumber
            && source_info.nFileIndexHigh == output_info.nFileIndexHigh
            && source_info.nFileIndexLow == output_info.nFileIndexLow;
    }
    if (source_file != INVALID_HANDLE_VALUE)
        CloseHandle(source_file);
    if (output_file != INVALID_HANDLE_VALUE)
        CloseHandle(output_file);
    return same;
#else
    struct stat source_stat;
    struct stat output_stat;
    return stat(source, &source_stat) == 0
        && stat(output, &output_stat) == 0
        && source_stat.st_dev == output_stat.st_dev
        && source_stat.st_ino == output_stat.st_ino;
#endif
}

bool
driver_binary_output_prepare(const DriverFlags *flags)
{
    char *output = driver_binary_output_resolve(flags);
    bool ok = true;
    if (output == NULL)
        return driver_binary_output_reject(flags,
            "binary output path could not be resolved");
    if (!pgy_path_is_safe(output)) {
        ok = driver_binary_output_reject(flags,
            "binary output path contains unsafe characters");
        goto done;
    }
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(output);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND)
            ok = driver_binary_output_reject(flags,
                "binary output path could not be inspected");
        goto done;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        ok = driver_binary_output_reject(flags,
            "binary output path names a directory");
        goto done;
    }
#else
    struct stat output_stat;
    if (lstat(output, &output_stat) != 0) {
        if (errno != ENOENT)
            ok = driver_binary_output_reject(flags,
                "binary output path could not be inspected");
        goto done;
    }
    if (S_ISDIR(output_stat.st_mode)) {
        ok = driver_binary_output_reject(flags,
            "binary output path names a directory");
        goto done;
    }
    if (!S_ISREG(output_stat.st_mode) && !S_ISLNK(output_stat.st_mode)) {
        ok = driver_binary_output_reject(flags,
            "binary output path is not a regular file");
        goto done;
    }
#endif
    if (driver_binary_output_is_source(flags->source_path, output)) {
        ok = driver_binary_output_reject(flags,
            "binary output path aliases the input source");
        goto done;
    }
    if (remove(output) != 0)
        ok = driver_binary_output_reject(flags,
            "previous binary output could not be removed");
done:
    free(output);
    return ok;
}

bool
driver_binary_output_prepare_for_target(const DriverFlags *flags)
{
    if (flags == NULL || flags->source_path == NULL
        || (!driver_plain_c_binary_target_requested(flags)
            && !driver_plain_llvm_binary_target_requested(flags)))
        return true;
    return driver_binary_output_prepare(flags);
}

int
driver_binary_output_refuse_invalid_args(const DriverFlags *flags)
{
    (void)driver_binary_output_prepare_for_target(flags);
    return 1;
}

/* The staging file sits in the destination's directory so the publishing
 * rename never crosses a filesystem, and keeps the destination's extension
 * so no toolchain appends one. On Windows a destination without an
 * extension is published as "<name>.exe", the name the linker writes. */
bool
driver_binary_output_publication_begin(const char *requested_path,
                                       DriverBinaryPublication *out)
{
    const char *base;
    const char *dot;
    size_t stem_length;
    char suffix[64];
    size_t size;

    if (out == NULL)
        return false;
    out->staging_path = NULL;
    out->target_path = NULL;
    if (requested_path == NULL)
        return false;
    base = strrchr(requested_path, '/');
#ifdef _WIN32
    {
        const char *back = strrchr(requested_path, '\\');
        if (back != NULL && (base == NULL || back > base))
            base = back;
    }
#endif
    base = base != NULL ? base + 1 : requested_path;
    dot = strrchr(base, '.');
#ifdef _WIN32
    if (dot == NULL || dot == base) {
        size = strlen(requested_path) + 5;
        out->target_path = malloc(size);
        if (out->target_path == NULL)
            return false;
        snprintf(out->target_path, size, "%s.exe", requested_path);
        base = out->target_path + (base - requested_path);
        dot = strrchr(base, '.');
    }
#endif
    if (out->target_path == NULL) {
        out->target_path = pergyra_strdup(requested_path);
        if (out->target_path == NULL)
            return false;
        base = out->target_path + (base - requested_path);
        dot = strrchr(base, '.');
    }
    if (dot == base)
        dot = NULL;
    stem_length = dot != NULL ? (size_t)(dot - out->target_path)
                              : strlen(out->target_path);
#ifdef _WIN32
    snprintf(suffix, sizeof(suffix), ".pgy-staging-%lu",
             (unsigned long)GetCurrentProcessId());
#else
    snprintf(suffix, sizeof(suffix), ".pgy-staging-%ld", (long)getpid());
#endif
    size = stem_length + strlen(suffix) + (dot != NULL ? strlen(dot) : 0) + 1;
    out->staging_path = malloc(size);
    if (out->staging_path == NULL) {
        driver_binary_output_publication_free(out);
        return false;
    }
    snprintf(out->staging_path, size, "%.*s%s%s", (int)stem_length,
             out->target_path, suffix, dot != NULL ? dot : "");
    (void)remove(out->staging_path);
    return true;
}

bool
driver_binary_output_publication_commit(const DriverFlags *flags,
                                        DriverBinaryPublication *publication)
{
    bool published;

    if (publication == NULL || publication->staging_path == NULL
        || publication->target_path == NULL)
        return false;
#ifdef _WIN32
    published = MoveFileExA(publication->staging_path, publication->target_path,
                            MOVEFILE_REPLACE_EXISTING) != 0;
#else
    published = rename(publication->staging_path, publication->target_path) == 0;
#endif
    if (published)
        return true;
    driver_binary_output_publication_abort(publication);
    driver_emit_stage_fail(flags, "binary_output",
        "binary publication failed",
        "the built binary could not be renamed over the requested output path");
    return false;
}

void
driver_binary_output_publication_abort(DriverBinaryPublication *publication)
{
    if (publication != NULL && publication->staging_path != NULL)
        (void)remove(publication->staging_path);
}

void
driver_binary_output_publication_free(DriverBinaryPublication *publication)
{
    if (publication == NULL)
        return;
    free(publication->staging_path);
    free(publication->target_path);
    publication->staging_path = NULL;
    publication->target_path = NULL;
}
