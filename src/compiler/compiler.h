#ifndef PERGYRA_COMPILER_H
#define PERGYRA_COMPILER_H

#include <stdbool.h>
#include "hir.h"

typedef struct
{
    bool  success;
    int   exit_code;
    char *error_message;
    char *c_output_path;
    char *binary_path;
} CompilerResult;

CompilerResult *compiler_emit_c(const HIRProgram *hir, const char *output_c_path);
CompilerResult *compiler_build_native(const HIRProgram *hir,
                                      const char *output_c_path,
                                      const char *output_binary_path,
                                      bool verbose);
int             compiler_run_binary(const char *binary_path, bool verbose);
void            compiler_result_destroy(CompilerResult *result);

#ifdef PGY_LLVM_ENABLED
/*
 * LLVM backend: AST → LLVM IR → object → link with GCC.
 */
CompilerResult *compiler_build_native_llvm(const HIRProgram *hir,
                                            const char *output_obj_path,
                                            const char *output_binary_path,
                                            bool verbose);

/*
 * Emit LLVM IR text to stdout (--emit-llvm mode).
 */
CompilerResult *compiler_emit_llvm_ir(const HIRProgram *hir, const char *module_name);
#endif

#endif
