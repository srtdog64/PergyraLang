/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "compiler/driver_app.h"
#include "compiler/repl.h"
#include "compiler/fmt.h"
#include "compiler/pkg.h"
#include "compiler/debugger.h"
#include "compiler/self_host_driver.h"
#include "compiler/driver_self_host_selection_owner.h"
#include "compiler/c_runner.h"
#include "compiler/llvm_runner.h"

typedef enum
{
    DRIVER_OPTION_NOOP,
    DRIVER_OPTION_BOOL,
    DRIVER_OPTION_BACKEND,
    DRIVER_OPTION_OPT_PROFILE,
    DRIVER_OPTION_DIAG_FORMAT,
    DRIVER_OPTION_RUNTIME,
    DRIVER_OPTION_HIR_DUMP
} DriverOptionKind;

typedef struct
{
    const char *name;
    DriverOptionKind kind;
    size_t offset;
    int value;
} DriverOptionSpec;

static const DriverOptionSpec k_driver_options[] = {
    { "--air", DRIVER_OPTION_BOOL, offsetof(DriverFlags, dump_air), true },
    { "--air-json", DRIVER_OPTION_BOOL, offsetof(DriverFlags, dump_air_json), true },
    { "--ast", DRIVER_OPTION_BOOL, offsetof(DriverFlags, dump_ast), true },
    { "--backend=c", DRIVER_OPTION_BACKEND, 0, BACKEND_C },
    { "--capability-manifest", DRIVER_OPTION_BOOL, offsetof(DriverFlags, dump_capability_manifest), true },
    { "--backend=llvm", DRIVER_OPTION_BACKEND, 0, BACKEND_LLVM },
    { "--compile", DRIVER_OPTION_NOOP, 0, 0 },
    { "--debug-lines", DRIVER_OPTION_BOOL, offsetof(DriverFlags, emit_debug_lines), true },
    { "--dir", DRIVER_OPTION_BOOL, offsetof(DriverFlags, dump_dir), true },
    { "--emit-c", DRIVER_OPTION_BOOL, offsetof(DriverFlags, emit_c_only), true },
    { "--emit-llvm", DRIVER_OPTION_BOOL, offsetof(DriverFlags, emit_llvm_ir), true },
    { "--error-format=json", DRIVER_OPTION_DIAG_FORMAT, 0, DIAG_FORMAT_JSON },
    { "--error-format=text", DRIVER_OPTION_DIAG_FORMAT, 0, DIAG_FORMAT_TEXT },
    { "--hir", DRIVER_OPTION_HIR_DUMP, 0, HIR_DUMP_SUMMARY },
    { "--hir-cfg", DRIVER_OPTION_HIR_DUMP, 0, HIR_DUMP_CFG },
    { "--hir-dom", DRIVER_OPTION_HIR_DUMP, 0, HIR_DUMP_DOM },
    { "--hir-ssa", DRIVER_OPTION_HIR_DUMP, 0, HIR_DUMP_SSA },
    { "--mir", DRIVER_OPTION_BOOL, offsetof(DriverFlags, dump_mir), true },
    { "--mir-json", DRIVER_OPTION_BOOL, offsetof(DriverFlags, dump_mir_json), true },
    { "--test-native-mir-json-oracle", DRIVER_OPTION_BOOL, offsetof(DriverFlags, test_native_mir_json_oracle), true },
    { "--machine-manifest-json", DRIVER_OPTION_BOOL, offsetof(DriverFlags, dump_machine_manifest_json), true },
    { "--opt=dev", DRIVER_OPTION_OPT_PROFILE, 0, PGY_OPT_DEV },
    { "--opt=release", DRIVER_OPTION_OPT_PROFILE, 0, PGY_OPT_RELEASE },
    { "--repl", DRIVER_OPTION_BOOL, offsetof(DriverFlags, repl), true },
    { "--rir", DRIVER_OPTION_BOOL, offsetof(DriverFlags, dump_rir), true },
    { "--rir-json", DRIVER_OPTION_BOOL, offsetof(DriverFlags, dump_rir_json), true },
    { "--run", DRIVER_OPTION_BOOL, offsetof(DriverFlags, do_run), true },
    { "--runtime=default", DRIVER_OPTION_RUNTIME, 0, RUNTIME_DEFAULT },
    { "--runtime=none", DRIVER_OPTION_RUNTIME, 0, RUNTIME_NONE },
    { "--tokens", DRIVER_OPTION_BOOL, offsetof(DriverFlags, dump_tokens), true },
    { "--verbose", DRIVER_OPTION_BOOL, offsetof(DriverFlags, verbose), true },
    { "-v", DRIVER_OPTION_BOOL, offsetof(DriverFlags, verbose), true },
};

static const DriverOptionSpec *
find_driver_option(const char *arg)
{
    if (arg == NULL)
        return NULL;

    for (size_t i = 0;
         i < sizeof(k_driver_options) / sizeof(k_driver_options[0]);
         i++) {
        if (strcmp(k_driver_options[i].name, arg) == 0)
            return &k_driver_options[i];
    }
    return NULL;
}

static void
apply_driver_option(DriverFlags *f, const DriverOptionSpec *option)
{
    if (f == NULL)
        return;
    if (option == NULL)
        return;

    switch (option->kind) {
    case DRIVER_OPTION_NOOP:
        break;
    case DRIVER_OPTION_BOOL:
        *(bool *)((char *)f + option->offset) = (bool)option->value;
        if (option->offset == offsetof(DriverFlags, emit_c_only))
            f->backend = BACKEND_C;
        if (option->offset == offsetof(DriverFlags, emit_llvm_ir))
            f->backend = BACKEND_LLVM;
        break;
    case DRIVER_OPTION_BACKEND:
        f->backend = (BackendKind)option->value;
        break;
    case DRIVER_OPTION_OPT_PROFILE:
        f->opt_profile = (PgyOptProfile)option->value;
        break;
    case DRIVER_OPTION_DIAG_FORMAT:
        f->diag_format = (DiagnosticFormat)option->value;
        break;
    case DRIVER_OPTION_RUNTIME:
        f->runtime_mode = (RuntimeMode)option->value;
        break;
    case DRIVER_OPTION_HIR_DUMP:
        f->dump_hir = true;
        f->hir_dump_mode = (HIRDumpMode)option->value;
        break;
    }
}

static DriverFlags
parse_args(int argc, char *argv[])
{
    DriverFlags f;
    memset(&f, 0, sizeof(f));
#ifdef PGY_LLVM_ENABLED
    f.backend = BACKEND_LLVM;
#else
    f.backend = BACKEND_C;
#endif
    f.opt_profile = PGY_OPT_RELEASE;
    f.hir_dump_mode = HIR_DUMP_SUMMARY;
    f.runtime_mode = RUNTIME_DEFAULT;

    for (int i = 1; i < argc; i++) {
        const DriverOptionSpec *option;

        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            driver_print_usage();
            exit(0);
        }

        option = find_driver_option(argv[i]);
        if (option != NULL) {
            apply_driver_option(&f, option);
        } else if (strncmp(argv[i], "--runtime=", 10) == 0) {
            fprintf(stderr,
                    "pgy: unknown runtime mode '%s' (expected --runtime=default or --runtime=none)\n",
                    argv[i] + 10);
            exit(1);
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "pgy: -o requires an argument\n");
                exit(1);
            }
            f.output_path = argv[++i];
        } else if (argv[i][0] != '-') {
            f.source_path = argv[i];
        } else {
            fprintf(stderr, "pgy: unknown option '%s'\n", argv[i]);
            exit(1);
        }
    }

    if (f.source_path == NULL && !f.repl
        && !f.dump_machine_manifest_json) {
        driver_print_usage();
        exit(1);
    }

#ifndef PGY_LLVM_ENABLED
    if (f.emit_llvm_ir) {
        fprintf(stderr, "pgy: this build was compiled without LLVM backend support\n");
        exit(1);
    }
#endif

    return f;
}

int
main(int argc, char *argv[])
{
    if (argc >= 2) {
        if (strcmp(argv[1], "--self-driver") == 0)
            return driver_run_self_host_command(argv[0], argc - 2, argv + 2);
        if (strcmp(argv[1], "fmt") == 0 && argc > 2 && argv[2][0] != '-')
            return driver_run_fmt_command(argc - 1, argv + 1);
        if (strcmp(argv[1], "fmt") == 0)
            return driver_run_pkg_command("fmt", argc - 2, argv + 2);
        if (strcmp(argv[1], "init") == 0)
            return driver_run_pkg_init(argc - 2, argv + 2);
        if (strcmp(argv[1], "check") == 0
            || strcmp(argv[1], "build") == 0
            || strcmp(argv[1], "run") == 0
            || strcmp(argv[1], "test") == 0
            || strcmp(argv[1], "lint") == 0
            || strcmp(argv[1], "prove") == 0
            || strcmp(argv[1], "package") == 0
            || strcmp(argv[1], "publish") == 0
            || strcmp(argv[1], "install") == 0)
            return driver_run_pkg_command(argv[1], argc - 2, argv + 2);
        if (strcmp(argv[1], "debug") == 0)
            return driver_run_debug_command(argc - 1, argv + 1);
        if (strcmp(argv[1], "scaffold") == 0)
            return driver_run_scaffold_command(argc - 1, argv + 1);
        if (strcmp(argv[1], "new") == 0) {
            char **sub_argv = calloc((size_t)argc + 2, sizeof(char *));
            if (sub_argv == NULL) {
                fprintf(stderr, "pgy: out of memory\n");
                return 1;
            }
            sub_argv[0] = argv[0];
            sub_argv[1] = "scaffold";
            sub_argv[2] = "project";
            for (int i = 2; i < argc; i++)
                sub_argv[i + 1] = argv[i];
            int rc = driver_run_scaffold_command(argc, sub_argv + 1);
            free(sub_argv);
            return rc;
        }
    }

    DriverFlags flags = parse_args(argc, argv);
    if (flags.repl)
        return repl_run();
    if (flags.test_native_mir_json_oracle) {
        if (flags.dump_mir_json) {
            fprintf(stderr,
                    "pgy: --test-native-mir-json-oracle is an exact test-only mode\n");
            return 1;
        }
        flags.test_native_mir_json_oracle = false;
        flags.dump_mir_json = true;
        if (!driver_self_host_mir_json_request_supported(&flags)) {
            fprintf(stderr,
                    "pgy: native MIR oracle options are outside the frozen test contract\n");
            return 1;
        }
        return driver_run_pipeline(&flags);
    }
    if (flags.dump_mir_json) {
        if (!driver_self_host_mir_json_request_supported(&flags)) {
            fprintf(stderr,
                    "pgy: --mir-json options are outside the installed self-host driver contract\n");
            return 1;
        }
        return driver_run_self_host_mir_json(argv[0], flags.source_path);
    }
    if (flags.emit_c_only) {
        if (flags.do_run
            || !driver_self_host_c_artifact_request_supported(&flags)) {
            fprintf(stderr,
                    "pgy: --emit-c options are outside the installed self-host driver contract\n");
            return 1;
        }
        return driver_run_self_host_c_emit_artifact(
            argv[0], flags.source_path, flags.output_path, flags.verbose);
    }
    if (driver_plain_c_binary_target_requested(&flags)) {
        if (!driver_self_host_c_artifact_request_supported(&flags)) {
            fprintf(stderr,
                    "pgy: C compile options are outside the installed self-host driver contract\n");
            return 1;
        }
        return c_runner_execute_installed_self_host_c(
            argv[0], &flags, NULL);
    }
    if (driver_plain_llvm_binary_target_requested(&flags)) {
        if (!driver_self_host_llvm_artifact_request_supported(&flags)) {
            fprintf(stderr,
                    "pgy: LLVM compile options are outside the installed self-host driver contract\n");
            return 1;
        }
        return llvm_runner_execute_installed_self_host_llvm(
            argv[0], &flags, NULL);
    }
    return driver_run_pipeline(&flags);
}
