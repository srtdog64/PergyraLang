/*
 * Public formatter host adapter.
 *
 * The installed Pergyra formatter owns tokenization, layout, parseability, and
 * stability. This boundary executes it exactly once, then owns only stdout,
 * byte comparison, and atomic in-place publication.
 */

#include "fmt.h"
#include "compiler_transient_artifact_workspace.h"
#include "path_utils.h"
#include "self_host_fmt_driver.h"
#include "self_host_driver.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
driver_run_fmt_command(const char *launcher_path, int argc, char *argv[])
{
    bool write_inplace = false;
    bool check_only = false;
    const char *path = NULL;
    const char *workspace_base;
    CompilerTransientArtifactWorkspace workspace = {0};
    char *canonical_path = NULL;
    char *source_dir = NULL;
    char *source = NULL;
    char *formatted = NULL;
    char *current = NULL;
    PathReplaceFileResult replace_result = PATH_REPLACE_ERROR;
    bool preserve_workspace = false;
    int rc = 1;

    if (argc < 2 || strcmp(argv[0], "fmt") != 0) {
        fprintf(stderr, "Usage: pgy fmt <file.pgy> [--write|--check]\n");
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--write") == 0 || strcmp(argv[i], "-w") == 0) {
            if (write_inplace) goto invalid_arguments;
            write_inplace = true;
        } else if (strcmp(argv[i], "--check") == 0) {
            if (check_only) goto invalid_arguments;
            check_only = true;
        } else if (argv[i][0] == '-' || path != NULL) {
            goto invalid_arguments;
        } else {
            path = argv[i];
        }
    }
    if (path == NULL || (write_inplace && check_only))
        goto invalid_arguments;

    canonical_path = driver_self_host_source_identity_path_dup(path);
    if (canonical_path == NULL) {
        fprintf(stderr, "pgy fmt: could not canonicalize '%s'\n", path);
        goto cleanup;
    }
    source = path_read_file(canonical_path);
    if (source == NULL) {
        fprintf(stderr, "pgy fmt: cannot read '%s'\n", path);
        goto cleanup;
    }
    source_dir = path_dirname_dup(canonical_path);
    if (source_dir == NULL) {
        fprintf(stderr, "pgy fmt: cannot resolve source directory\n");
        goto cleanup;
    }
    workspace_base = source_dir;
    if (!write_inplace) {
        const char *temporary_base = getenv("TMPDIR");
#ifdef _WIN32
        if (temporary_base == NULL || temporary_base[0] == '\0')
            temporary_base = getenv("TEMP");
#else
        if (temporary_base == NULL || temporary_base[0] == '\0')
            temporary_base = "/tmp";
#endif
        if (temporary_base != NULL && temporary_base[0] != '\0')
            workspace_base = temporary_base;
    }
    if (!compiler_transient_artifact_workspace_open(
            canonical_path, workspace_base, ".fmt.tmp", ".fmt.previous",
            &workspace)) {
        fprintf(stderr, "pgy fmt: cannot create private artifact workspace\n");
        goto cleanup;
    }

    rc = driver_materialize_self_host_format_artifact(
        launcher_path, canonical_path, workspace.primary_path);
    if (rc != 0) goto cleanup;
    formatted = path_read_file(workspace.primary_path);
    if (formatted == NULL) {
        fprintf(stderr, "pgy fmt: cannot read verified temporary output\n");
        rc = 1;
        goto cleanup;
    }
    current = path_read_file(canonical_path);
    if (current == NULL || strcmp(source, current) != 0) {
        fprintf(stderr,
                "pgy fmt: source changed while formatting; refusing stale output\n");
        rc = 1;
        goto cleanup;
    }

    if (!write_inplace && !check_only) {
        if (fputs(formatted, stdout) == EOF) {
            fprintf(stderr, "pgy fmt: failed to write formatted output\n");
            rc = 1;
        } else {
            rc = 0;
        }
        goto cleanup;
    }

    if (check_only) {
        if (strcmp(source, formatted) != 0) {
            fprintf(stderr, "pgy fmt: '%s' needs formatting\n", path);
            rc = 1;
        } else {
            rc = 0;
        }
        goto cleanup;
    }

    if (strcmp(source, formatted) == 0) {
        printf("pgy fmt: '%s' already formatted\n", path);
        rc = 0;
        goto cleanup;
    }
    replace_result = path_replace_file_atomic_if_unchanged(
        workspace.primary_path, canonical_path, workspace.secondary_path,
        source);
    if (replace_result == PATH_REPLACE_SOURCE_CHANGED) {
        fprintf(stderr,
                "pgy fmt: source changed while formatting; refusing stale output\n");
        rc = 1;
        goto cleanup;
    }
    if (replace_result == PATH_REPLACE_RECOVERY_REQUIRED) {
        preserve_workspace = true;
        fprintf(stderr,
                "pgy fmt: source changed during final publication; recovery "
                "artifacts preserved at '%s'\n",
                workspace.directory);
        rc = 1;
        goto cleanup;
    }
    if (replace_result != PATH_REPLACE_OK) {
        fprintf(stderr,
                "pgy fmt: failed to atomically replace '%s' with formatted output\n",
                path);
        rc = 1;
        goto cleanup;
    }
    printf("pgy fmt: formatted '%s'\n", path);
    rc = 0;
    goto cleanup;

invalid_arguments:
    fprintf(stderr, "Usage: pgy fmt <file.pgy> [--write|--check]\n");
cleanup:
    free(current);
    free(formatted);
    free(source);
    free(source_dir);
    free(canonical_path);
    if (!preserve_workspace)
        compiler_transient_artifact_workspace_close(&workspace);
    return rc;
}
