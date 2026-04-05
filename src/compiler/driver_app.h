/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifndef PGY_DRIVER_APP_H
#define PGY_DRIVER_APP_H

#include <stdbool.h>
#include "compiler.h"

typedef enum
{
    BACKEND_C,
    BACKEND_LLVM
} BackendKind;

typedef struct
{
    const char *source_path;
    const char *output_path;
    bool        emit_c_only;
    bool        emit_llvm_ir;
    bool        do_run;
    bool        dump_tokens;
    bool        dump_ast;
    bool        dump_dir;
    bool        dump_rir;
    bool        dump_mir;
    bool        dump_hir;
    HIRDumpMode hir_dump_mode;
    bool        verbose;
    bool        repl;
    BackendKind backend;
    PgyOptProfile opt_profile;
} DriverFlags;

int  driver_run_pipeline(const DriverFlags *flags);
int  driver_run_scaffold_command(int argc, char *argv[]);
void driver_print_usage(void);

#endif /* PGY_DRIVER_APP_H */
