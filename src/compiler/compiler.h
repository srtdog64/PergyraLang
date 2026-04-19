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
    /* Stable diagnostic code (owning, e.g. "PGY_MIR_UNRESOLVED_LOCAL").
     * NULL when the failing site has not been assigned a code. Propagated
     * from TranspilerCtx.backend_error_code / LLVMGenCtx.error_code. */
    char *error_code;
    /* Optional hint tags (owning strdups). cause_ir = IR-level origin
     * (e.g. "llvm:result_spec:capacity_exceeded"); fix_source =
     * source-level repair action token (e.g. "reuse-shared-error-enum").
     * Both NULL when the failing site did not provide them. Propagated
     * from TranspilerCtx / LLVMGenCtx hint fields. */
    char *error_cause_ir;
    char *error_fix_source;
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
