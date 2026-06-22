/*
 * pgy package command owner.
 *
 * Seashell manifest parsing and lock validation live in pkg_manifest.c.
 * This file owns command dispatch and driver pipeline invocation only.
 */

#include "pkg.h"

#include "driver_app.h"
#include "fmt.h"
#include "pkg_manifest.h"

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
pkg_run_entry(const PgyPackageManifest *manifest,
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
    rc = driver_run_pipeline(&flags);
    if (rc == 0)
        printf("pgy %s: %s ok\n", verb, entry_path);
    free(entry_path);
    return rc;
}

static int
pkg_run_fmt_entry(const PgyPackageManifest *manifest, int argc, char *argv[])
{
    char *entry_path;
    char *fmt_argv[4];
    int fmt_argc = 0;
    bool write = false;
    int rc;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--write") == 0 || strcmp(argv[i], "-w") == 0)
            write = true;
        else if (strcmp(argv[i], "--check") != 0) {
            fprintf(stderr, "pgy fmt: package mode accepts --check or --write\n");
            return 1;
        }
    }

    entry_path = pgy_package_manifest_entry_path_dup(manifest, false);
    if (entry_path == NULL)
        return 1;
    fmt_argv[fmt_argc++] = "fmt";
    fmt_argv[fmt_argc++] = entry_path;
    fmt_argv[fmt_argc++] = write ? "--write" : "--check";
    rc = driver_run_fmt_command(fmt_argc, fmt_argv);
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
driver_run_pkg_command(const char *verb, int argc, char *argv[])
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
        rc = pkg_run_entry(&manifest, "check", false, true, false);
    } else if (strcmp(verb, "build") == 0) {
        rc = pkg_run_entry(&manifest, "build", false, false, false);
    } else if (strcmp(verb, "run") == 0) {
        rc = pkg_run_entry(&manifest, "run", false, false, true);
    } else if (strcmp(verb, "test") == 0) {
        rc = pkg_run_entry(&manifest, "test", true, false, true);
    } else if (strcmp(verb, "lint") == 0) {
        rc = pkg_run_entry(&manifest, "lint", false, true, false);
    } else if (strcmp(verb, "prove") == 0) {
        rc = pkg_run_entry(&manifest, "prove", false, true, false);
        if (rc == 0)
            printf("pgy prove: package evidence preflight ok (not a theorem)\n");
    } else if (strcmp(verb, "fmt") == 0) {
        rc = pkg_run_fmt_entry(&manifest, argc, argv);
    } else if (strcmp(verb, "package") == 0) {
        rc = pkg_run_entry(&manifest, "package-check", false, true, false);
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
