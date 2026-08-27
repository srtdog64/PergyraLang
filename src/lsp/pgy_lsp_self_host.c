/*
 * Owns public LSP handoff to the installed Pergyra-built sibling. The native
 * diagnostics and live-session implementations remain explicit oracles only.
 */

#include "pgy_lsp_self_host.h"

#include <stdio.h>
#include <stdlib.h>

#include "../compiler/compiler_process.h"
#include "../compiler/path_utils.h"

static char *
pgy_lsp_resolve_self_host_binary(const char *launcher_path)
{
    const char *override = getenv("PGY_SELF_LSP_BIN");
    char *directory;
    char *candidate;
    char *resolved;

    if (override != NULL && override[0] != '\0')
        return path_resolve_runnable_binary(override);

    directory = path_dirname_dup(launcher_path);
    if (directory == NULL)
        return NULL;
    candidate = path_join_dup(directory, "pgy-self-lsp");
    free(directory);
    if (candidate == NULL)
        return NULL;
    resolved = path_resolve_runnable_binary(candidate);
    free(candidate);
    return resolved;
}

static char *
pgy_lsp_require_self_host_binary(const char *launcher_path,
                                 const char *purpose)
{
    char *binary = pgy_lsp_resolve_self_host_binary(launcher_path);

    if (binary != NULL && path_file_exists(binary))
        return binary;
    fprintf(stderr,
            "pgy-lsp: self-host %s is unavailable; "
            "run 'make self-host-lsp' or set PGY_SELF_LSP_BIN\n",
            purpose != NULL ? purpose : "owner");
    free(binary);
    return NULL;
}

int
pgy_lsp_run_self_host_diagnostics(const char *launcher_path,
                                  const char *source_path)
{
    const char *child_argv[3];
    char *binary;
    int rc;

    if (launcher_path == NULL || source_path == NULL || source_path[0] == '\0') {
        fprintf(stderr,
                "pgy-lsp: self-host diagnostics requires a source path\n");
        return 1;
    }

    binary = pgy_lsp_require_self_host_binary(launcher_path, "diagnostics");
    if (binary == NULL)
        return 1;

    child_argv[0] = binary;
    child_argv[1] = source_path;
    child_argv[2] = NULL;
    rc = pgy_exec_argv(child_argv, false);
    if (rc != 0) {
        fprintf(stderr,
                "pgy-lsp: self-host diagnostics failed (exit %d) for %s\n",
                rc, source_path);
    }
    free(binary);
    return rc;
}

int
pgy_lsp_run_self_host_session(const char *launcher_path)
{
    const char *child_argv[2];
    char *binary;
    int rc;

    if (launcher_path == NULL) {
        fprintf(stderr, "pgy-lsp: self-host live session has no launcher\n");
        return 1;
    }
    binary = pgy_lsp_require_self_host_binary(launcher_path, "live session");
    if (binary == NULL)
        return 1;

    child_argv[0] = binary;
    child_argv[1] = NULL;
    rc = pgy_exec_argv(child_argv, false);
    if (rc != 0)
        fprintf(stderr, "pgy-lsp: self-host live session failed (exit %d)\n", rc);
    free(binary);
    return rc;
}
