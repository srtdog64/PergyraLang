#ifndef PERGYRA_COMPILER_H
#define PERGYRA_COMPILER_H

#include <stdbool.h>
#include "hir.h"
#include "dir.h"
#include "rir.h"
#include "mir.h"

typedef enum
{
    PGY_OPT_DEV,
    PGY_OPT_RELEASE
} PgyOptProfile;

typedef struct
{
    double codegen;
    double native_compile;
    double link;
} CompilerBackendTimings;

typedef struct
{
    bool  success;
    int   exit_code;
    char *error_message;
    char *c_output_path;
    char *binary_path;
    CompilerBackendTimings backend_timings;
} CompilerResult;

typedef struct
{
    const HIRProgram *hir;
    const DIRProgram *dir;
    const RIRProgram *rir;
    const MIRProgram *mir;
} CompilerIRBundle;

CompilerResult *compiler_emit_c(const CompilerIRBundle *bundle, const char *output_c_path);
CompilerResult *compiler_build_native(const CompilerIRBundle *bundle,
                                      const char *output_c_path,
                                      const char *output_binary_path,
                                      bool verbose,
                                      PgyOptProfile opt_profile);
int             compiler_run_binary(const char *binary_path, bool verbose);
void            compiler_result_destroy(CompilerResult *result);

/*
 * LLVM backend: HIR → LLVM IR → object → link with GCC.
 */
CompilerResult *compiler_build_native_llvm(const CompilerIRBundle *bundle,
                                            const char *output_obj_path,
                                            const char *output_binary_path,
                                            bool verbose,
                                            PgyOptProfile opt_profile);

/*
 * Emit LLVM IR text to stdout (--emit-llvm mode).
 */
CompilerResult *compiler_emit_llvm_ir(const CompilerIRBundle *bundle, const char *module_name);

/*
 * Emit LLVM IR text to a file.
 */
CompilerResult *compiler_emit_llvm_ir_to_file(const CompilerIRBundle *bundle,
                                              const char *module_name,
                                              const char *output_ir_path);

#endif
