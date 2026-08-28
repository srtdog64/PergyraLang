/*
 * pgy package command owner.
 *
 * Seashell manifest parsing and lock validation live in pkg_manifest.c.
 * This file owns command dispatch and the manifest-to-installed-artifact
 * handoff. The native pipeline remains an explicit bootstrap/test opt-out.
 */

#include "pkg.h"

#include "compiler_transient_artifact_workspace.h"
#include "c_runner.h"
#include "driver_app.h"
#include "fmt.h"
#include "llvm_runner.h"
#include "pkg_manifest.h"
#include "self_host_mir_artifact_owner.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static DriverFlags
pkg_driver_flags(const PgyPackageManifest *manifest,
                 const char *source_path,
                 bool check_only,
                 bool do_run)
{
    DriverFlags flags;

    memset(&flags, 0, sizeof(flags));
    flags.source_path = source_path;
    flags.check_only = check_only;
    flags.do_run = do_run;
    flags.backend = manifest != NULL ? manifest->backend : BACKEND_C;
    flags.opt_profile = PGY_OPT_RELEASE;
    flags.hir_dump_mode = HIR_DUMP_SUMMARY;
    flags.runtime_mode = RUNTIME_DEFAULT;
    flags.diag_format = DIAG_FORMAT_TEXT;
    return flags;
}

static int
pkg_verify_entry_with_installed_self_host(const char *launcher_path,
                                          const char *source_path)
{
    CompilerTransientArtifactWorkspace workspace;
    int rc;

    if (!compiler_transient_artifact_workspace_open(
            source_path, ".", ".mir.json", NULL, &workspace)) {
        fprintf(stderr,
                "pgy: could not create a private package verification workspace\n");
        return 1;
    }
    rc = driver_materialize_self_host_mir_artifact(
        launcher_path, source_path, workspace.primary_path, false);
    compiler_transient_artifact_workspace_close(&workspace);
    return rc;
}

static int
pkg_run_entry(const char *launcher_path,
              bool native_pipeline,
              const PgyPackageManifest *manifest,
              const char *verb,
              bool test_target,
              bool check_only,
              bool do_run)
{
    char *entry_path;
    DriverFlags flags;
    int rc;

    entry_path = pgy_package_manifest_entry_path_dup(manifest, test_target);
    if (entry_path == NULL) {
        fprintf(stderr, "pgy %s: out of memory\n", verb);
        return 1;
    }
    flags = pkg_driver_flags(manifest, entry_path, check_only, do_run);
    if (native_pipeline) {
        rc = driver_run_pipeline(&flags);
    } else if (check_only) {
        rc = pkg_verify_entry_with_installed_self_host(
            launcher_path, entry_path);
    } else if (flags.backend == BACKEND_LLVM) {
        rc = llvm_runner_execute_installed_self_host_llvm(
            launcher_path, &flags, NULL);
    } else {
        rc = c_runner_execute_installed_self_host_c(
            launcher_path, &flags, NULL);
    }
    if (rc == 0)
        printf("pgy %s: %s ok\n", verb, entry_path);
    free(entry_path);
    return rc;
}

static int
pkg_run_fmt_entry(const char *launcher_path,
                  const PgyPackageManifest *manifest,
                  int argc,
                  char *argv[])
{
    char *entry_path;
    char *fmt_argv[4];
    int fmt_argc = 0;
    bool write = false;
    bool check = false;
    int rc;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--write") == 0 || strcmp(argv[i], "-w") == 0) {
            if (write) {
                fprintf(stderr, "pgy fmt: duplicate --write option\n");
                return 1;
            }
            write = true;
        } else if (strcmp(argv[i], "--check") == 0) {
            if (check) {
                fprintf(stderr, "pgy fmt: duplicate --check option\n");
                return 1;
            }
            check = true;
        } else {
            fprintf(stderr, "pgy fmt: package mode accepts --check or --write\n");
            return 1;
        }
    }
    if (write && check) {
        fprintf(stderr, "pgy fmt: --write and --check are mutually exclusive\n");
        return 1;
    }

    entry_path = pgy_package_manifest_entry_path_dup(manifest, false);
    if (entry_path == NULL)
        return 1;
    fmt_argv[fmt_argc++] = "fmt";
    fmt_argv[fmt_argc++] = entry_path;
    fmt_argv[fmt_argc++] = write ? "--write" : "--check";
    rc = driver_run_fmt_command(launcher_path, fmt_argc, fmt_argv);
    free(entry_path);
    return rc;
}

int
driver_run_pkg_init(int argc, char *argv[])
{
    const char *name = "my-project";
    PgyPackageManifest manifest;
    FILE *f;
    FILE *m;

    if (argc > 0 && argv[0][0] != '-')
        name = argv[0];
    if (!pgy_package_manifest_package_name_is_valid(name)) {
        fprintf(stderr,
            "pgy init: package name must use only letters, digits, '.', '_', or '-'\n");
        return 1;
    }

    f = fopen("pgy.toml", "rb");
    if (f != NULL) {
        fclose(f);
        fprintf(stderr, "pgy init: pgy.toml already exists\n");
        return 1;
    }

    f = fopen("pgy.toml", "wb");
    if (f == NULL) {
        fprintf(stderr, "pgy init: cannot create pgy.toml\n");
        return 1;
    }
    fprintf(f, pgy_package_manifest_template(), name);
    fclose(f);
    printf("pgy init: created pgy.toml for '%s'\n", name);

    m = fopen("main.pgy", "rb");
    if (m != NULL) {
        fclose(m);
    } else {
        m = fopen("main.pgy", "wb");
        if (m != NULL) {
            fprintf(m,
                "func Main() -> Void\n"
                "{\n"
                "    Log(\"Hello, %s!\");\n"
                "}\n"
                "\n",
                name);
            fclose(m);
            printf("pgy init: created main.pgy\n");
        }
    }

    if (pgy_package_manifest_load(&manifest, "pgy.toml")) {
        int rc = pgy_package_manifest_write_lock(&manifest);
        pgy_package_manifest_destroy(&manifest);
        return rc;
    }
    return 1;
}

int
driver_run_pkg_command(const char *launcher_path,
                       bool native_pipeline,
                       const char *verb,
                       int argc,
                       char *argv[])
{
    PgyPackageManifest manifest;
    int rc = 1;

    if (verb == NULL)
        return 1;
    if (strcmp(verb, "install") == 0) {
        fprintf(stderr,
            "pgy install: dependency version solving and registry install are out-of-beta. "
            "Use file imports and compiler-known `use` modules for the beta surface.\n");
        return 1;
    }
    if (argc > 0 && strcmp(verb, "fmt") != 0) {
        fprintf(stderr,
            "pgy %s: package command arguments are not supported yet\n",
            verb);
        return 1;
    }

    if (!pgy_package_manifest_load(&manifest, "pgy.toml"))
        return 1;
    if (strcmp(verb, "package") != 0
        && !pgy_package_manifest_verify_existing_lock(&manifest, "pgy.lock"))
        goto cleanup;

    if (strcmp(verb, "check") == 0) {
        rc = pkg_run_entry(launcher_path, native_pipeline,
            &manifest, "check", false, true, false);
    } else if (strcmp(verb, "build") == 0) {
        rc = pkg_run_entry(launcher_path, native_pipeline,
            &manifest, "build", false, false, false);
    } else if (strcmp(verb, "run") == 0) {
        rc = pkg_run_entry(launcher_path, native_pipeline,
            &manifest, "run", false, false, true);
    } else if (strcmp(verb, "test") == 0) {
        rc = pkg_run_entry(launcher_path, native_pipeline,
            &manifest, "test", true, false, true);
    } else if (strcmp(verb, "lint") == 0) {
        rc = pkg_run_entry(launcher_path, native_pipeline,
            &manifest, "lint", false, true, false);
    } else if (strcmp(verb, "prove") == 0) {
        rc = pkg_run_entry(launcher_path, native_pipeline,
            &manifest, "prove", false, true, false);
        if (rc == 0)
            printf("pgy prove: package evidence preflight ok (not a theorem)\n");
    } else if (strcmp(verb, "fmt") == 0) {
        rc = pkg_run_fmt_entry(launcher_path, &manifest, argc, argv);
    } else if (strcmp(verb, "package") == 0) {
        rc = pkg_run_entry(launcher_path, native_pipeline,
            &manifest, "package-check", false, true, false);
        if (rc == 0)
            rc = pgy_package_manifest_write_lock(&manifest);
    } else if (strcmp(verb, "publish") == 0) {
        fprintf(stderr,
            "pgy publish: registry publishing is out-of-beta; run `pgy package` for the local deterministic lock artifact.\n");
        rc = 1;
    } else {
        fprintf(stderr, "pgy: unknown package command '%s'\n", verb);
        rc = 1;
    }

cleanup:
    pgy_package_manifest_destroy(&manifest);
    return rc;
}
