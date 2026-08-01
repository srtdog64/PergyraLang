#include "compiler_transient_artifact_workspace.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define getpid _getpid
#define pgy_mkdir_private(path) _mkdir(path)
#define pgy_rmdir(path)         _rmdir(path)
#else
#include <unistd.h>
#define pgy_mkdir_private(path) mkdir((path), 0700)
#define pgy_rmdir(path)         rmdir((path))
#endif

static bool
compiler_transient_artifact_workspace_make_directory(
    const char *base, char *out, size_t out_cap)
{
    unsigned long nonce = (unsigned long)getpid()
        ^ ((unsigned long)time(NULL) << 8);

    for (int attempt = 0; attempt < 64; attempt++) {
        int written = snprintf(out, out_cap, "%s/pgy-%lx",
                               base, nonce +
                               (unsigned long)attempt * 2654435761UL);
        if (written < 0 || (size_t)written >= out_cap)
            return false;
        if (pgy_mkdir_private(out) == 0)
            return true;
        if (errno != EEXIST)
            return false;
    }
    return false;
}

static bool
compiler_transient_artifact_workspace_path(
    const char *directory, const char *stem, const char *suffix,
    char *out, size_t out_cap)
{
    int written;

    if (suffix == NULL || suffix[0] == '\0') {
        out[0] = '\0';
        return true;
    }
    written = snprintf(out, out_cap, "%s/%s%s", directory, stem, suffix);
    return written >= 0 && (size_t)written < out_cap;
}

bool
compiler_transient_artifact_workspace_open(
    const char *source_path,
    const char *base_directory,
    const char *primary_suffix,
    const char *secondary_suffix,
    CompilerTransientArtifactWorkspace *workspace)
{
    const char *base;
    const char *sep;
    const char *dot;
    size_t stem_length;
    char stem[256];

    if (source_path == NULL || base_directory == NULL ||
        primary_suffix == NULL || workspace == NULL)
        return false;
    memset(workspace, 0, sizeof(*workspace));
    if (!compiler_transient_artifact_workspace_make_directory(
            base_directory, workspace->directory,
            sizeof(workspace->directory)))
        return false;

    base = source_path;
    sep = strrchr(base, '/');
#ifdef _WIN32
    {
        const char *backslash = strrchr(base, '\\');
        if (backslash != NULL && (sep == NULL || backslash > sep))
            sep = backslash;
    }
#endif
    if (sep != NULL)
        base = sep + 1;
    dot = strrchr(base, '.');
    stem_length = dot != NULL ? (size_t)(dot - base) : strlen(base);
    if (stem_length > sizeof(stem) - 1)
        stem_length = sizeof(stem) - 1;
    memcpy(stem, base, stem_length);
    stem[stem_length] = '\0';

    if (!compiler_transient_artifact_workspace_path(
            workspace->directory, stem, primary_suffix,
            workspace->primary_path, sizeof(workspace->primary_path)) ||
        !compiler_transient_artifact_workspace_path(
            workspace->directory, stem, secondary_suffix,
            workspace->secondary_path, sizeof(workspace->secondary_path))) {
        pgy_rmdir(workspace->directory);
        memset(workspace, 0, sizeof(*workspace));
        return false;
    }
    workspace->active = true;
    return true;
}

void
compiler_transient_artifact_workspace_close(
    CompilerTransientArtifactWorkspace *workspace)
{
    if (workspace == NULL || !workspace->active)
        return;
    if (workspace->secondary_path[0] != '\0')
        remove(workspace->secondary_path);
    if (workspace->primary_path[0] != '\0')
        remove(workspace->primary_path);
    pgy_rmdir(workspace->directory);
    memset(workspace, 0, sizeof(*workspace));
}
