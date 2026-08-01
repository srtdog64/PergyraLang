#ifndef PGY_SELF_HOST_LLVM_DRIVER_H
#define PGY_SELF_HOST_LLVM_DRIVER_H

#include <stdbool.h>

int driver_materialize_self_host_llvm_artifacts(
    const char *launcher_path,
    const char *source_path,
    const char *mir_output_path,
    const char *llvm_output_path,
    bool verbose);

#endif /* PGY_SELF_HOST_LLVM_DRIVER_H */
