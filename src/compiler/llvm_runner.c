/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "llvm_runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "compiler.h"
#include "path_utils.h"

#ifdef PGY_LLVM_ENABLED

int
llvm_runner_execute(const DriverFlags *flags, const CompilerIRBundle *bundle)
{
    if (flags->emit_llvm_ir) {
        CompilerResult *result = flags->output_path != NULL
            ? compiler_emit_llvm_ir_to_file(bundle, "pergyra_module", flags->output_path)
            : compiler_emit_llvm_ir(bundle, "pergyra_module");
        if (result == NULL || !result->success) {
            fprintf(stderr, "pgy: LLVM IR generation failed: %s\n",
                    result != NULL ? result->error_message : "out of memory");
            compiler_result_destroy(result);
            return 1;
        }

        if (flags->output_path != NULL)
            printf("pgy: wrote %s\n", flags->output_path);
        compiler_result_destroy(result);
        return 0;
    }

    char *bin_path = flags->output_path != NULL
        ? pergyra_strdup(flags->output_path)
        : path_default_binary(flags->source_path);
    char *obj_path = bin_path != NULL
        ? path_replace_extension(bin_path, ".o") : NULL;
    CompilerResult *result;

    if (bin_path == NULL || obj_path == NULL) {
        fprintf(stderr, "pgy: out of memory\n");
        free(bin_path);
        free(obj_path);
        return 1;
    }

    result = compiler_build_native_llvm(bundle, obj_path, bin_path, flags->verbose,
                                        flags->opt_profile);
    if (result == NULL || !result->success) {
        fprintf(stderr, "pgy: LLVM compile failed: %s\n",
                result != NULL ? result->error_message : "out of memory");
        compiler_result_destroy(result);
        free(obj_path);
        free(bin_path);
        return 1;
    }

    printf("pgy: compiled (LLVM) → %s\n", bin_path);
    int exit_code = 0;
    if (flags->do_run) {
        exit_code = compiler_run_binary(bin_path, flags->verbose);
        if (exit_code != 0)
            fprintf(stderr, "pgy: program exited with code %d\n", exit_code);
    }

    compiler_result_destroy(result);
    free(obj_path);
    free(bin_path);
    return exit_code;
}

#else /* !PGY_LLVM_ENABLED */

int
llvm_runner_execute(const DriverFlags *flags, const CompilerIRBundle *bundle)
{
    (void)flags;
    (void)bundle;
    fprintf(stderr, "pgy: LLVM backend not available in this build\n");
    return 1;
}

#endif
