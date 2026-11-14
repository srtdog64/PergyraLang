#ifndef PGY_SELF_HOST_LLVM_DRIVER_H
#define PGY_SELF_HOST_LLVM_DRIVER_H

#include <stdbool.h>

int driver_materialize_self_host_llvm_artifact(
    const char *launcher_path,
    const char *source_path,
    const char *llvm_output_path,
    bool verbose,
    bool emit_json_diagnostic);

#endif /* PGY_SELF_HOST_LLVM_DRIVER_H */
