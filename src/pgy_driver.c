/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler/driver_app.h"
#include "compiler/repl.h"
#include "compiler/fmt.h"
#include "compiler/pkg.h"
#include "compiler/debugger.h"

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

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            driver_print_usage();
            exit(0);
        } else if (strcmp(argv[i], "--compile") == 0) {
            continue;
        } else if (strcmp(argv[i], "--emit-c") == 0) {
            f.emit_c_only = true;
            f.backend = BACKEND_C;
        } else if (strcmp(argv[i], "--backend=llvm") == 0) {
            f.backend = BACKEND_LLVM;
        } else if (strcmp(argv[i], "--backend=c") == 0) {
            f.backend = BACKEND_C;
        } else if (strcmp(argv[i], "--emit-llvm") == 0) {
            f.emit_llvm_ir = true;
            f.backend = BACKEND_LLVM;
        } else if (strcmp(argv[i], "--opt=dev") == 0) {
            f.opt_profile = PGY_OPT_DEV;
        } else if (strcmp(argv[i], "--opt=release") == 0) {
            f.opt_profile = PGY_OPT_RELEASE;
        } else if (strcmp(argv[i], "--run") == 0) {
            f.do_run = true;
        } else if (strcmp(argv[i], "--tokens") == 0) {
            f.dump_tokens = true;
        } else if (strcmp(argv[i], "--ast") == 0) {
            f.dump_ast = true;
        } else if (strcmp(argv[i], "--dir") == 0) {
            f.dump_dir = true;
        } else if (strcmp(argv[i], "--rir") == 0) {
            f.dump_rir = true;
        } else if (strcmp(argv[i], "--mir") == 0) {
            f.dump_mir = true;
        } else if (strcmp(argv[i], "--hir") == 0) {
            f.dump_hir = true;
            f.hir_dump_mode = HIR_DUMP_SUMMARY;
        } else if (strcmp(argv[i], "--hir-cfg") == 0) {
            f.dump_hir = true;
            f.hir_dump_mode = HIR_DUMP_CFG;
        } else if (strcmp(argv[i], "--hir-dom") == 0) {
            f.dump_hir = true;
            f.hir_dump_mode = HIR_DUMP_DOM;
        } else if (strcmp(argv[i], "--hir-ssa") == 0) {
            f.dump_hir = true;
            f.hir_dump_mode = HIR_DUMP_SSA;
        } else if (strcmp(argv[i], "--repl") == 0) {
            f.repl = true;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            f.verbose = true;
        } else if (strcmp(argv[i], "--error-format=json") == 0) {
            f.diag_format = DIAG_FORMAT_JSON;
        } else if (strcmp(argv[i], "--error-format=text") == 0) {
            f.diag_format = DIAG_FORMAT_TEXT;
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

    if (f.source_path == NULL && !f.repl) {
        driver_print_usage();
        exit(1);
    }

#ifndef PGY_LLVM_ENABLED
    if (f.backend == BACKEND_LLVM || f.emit_llvm_ir) {
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
        if (strcmp(argv[1], "fmt") == 0)
            return driver_run_fmt_command(argc - 1, argv + 1);
        if (strcmp(argv[1], "init") == 0)
            return driver_run_pkg_init(argc - 2, argv + 2);
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
    return driver_run_pipeline(&flags);
}
