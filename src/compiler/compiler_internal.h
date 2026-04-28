#ifndef PERGYRA_COMPILER_INTERNAL_H
#define PERGYRA_COMPILER_INTERNAL_H

#include "compiler.h"

#ifndef PGY_SRC_DIR
#define PGY_SRC_DIR "src"
#endif

#ifndef PGY_RUNTIME_DIR
#define PGY_RUNTIME_DIR "src/runtime"
#endif

#ifndef PGY_RUNTIME_LIB_C
#define PGY_RUNTIME_LIB_C "src/runtime/pgy_runtime_lib.c"
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

#endif /* PERGYRA_COMPILER_INTERNAL_H */
