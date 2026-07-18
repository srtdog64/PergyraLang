#ifndef PERGYRA_COMPILER_INTERNAL_H
#define PERGYRA_COMPILER_INTERNAL_H

#include "compiler.h"
#include "verified_projection_plan.h"

#ifndef PGY_SRC_DIR
#define PGY_SRC_DIR "src"
#endif

#ifndef PGY_RUNTIME_DIR
#define PGY_RUNTIME_DIR "src/runtime"
#endif

#ifndef PGY_RUNTIME_LIB_C
#define PGY_RUNTIME_LIB_C "src/runtime/pgy_runtime_lib.c"
#endif

/* C-leg extern runtime object TU (inline->extern, docs/189 C14). The TU owns
 * PGY_RUNTIME_EXTERN_DEFS so it holds one external-linkage definition of every
 * converted runtime body; the emitted C in extern mode links it.
 *
 * Anchored to PGY_RUNTIME_DIR (absolute when the driver is built) so the cc
 * `-c` argument is an absolute path, matching the -I include dirs: the cext
 * object then rebuilds from any working directory, not only the repo root
 * (docs/190 B1). The relative fallback stays identical for un-defined builds. */
#ifndef PGY_RUNTIME_CEXT_LIB_C
#define PGY_RUNTIME_CEXT_LIB_C PGY_RUNTIME_DIR "/pgy_runtime_cext_lib.c"
#endif

#ifdef _WIN32
#define PGY_CFLAGS_THREAD_LIB "-lwinpthread"
#define PGY_CFLAGS_THREAD_FLAG "-pthread"
#else
#define PGY_CFLAGS_THREAD_LIB "-lpthread"
#define PGY_CFLAGS_THREAD_FLAG "-pthread"
#endif

CompilerResult *compiler_error(const char *message);
CompilerResult *compiler_error_with_code(const char *message, const char *code);
CompilerResult *compiler_error_full(const char *message,
                                    const char *code,
                                    const char *cause_ir,
                                    const char *fix_source);
CompilerResult *compiler_success(const char *output_c_path,
                                 const char *output_binary_path);
bool compiler_result_bind_artifact_identity(
    CompilerResult *result,
    const PgyVerifiedProjectionPlanRow *plan,
    const char *artifact_kind);

#endif /* PERGYRA_COMPILER_INTERNAL_H */
