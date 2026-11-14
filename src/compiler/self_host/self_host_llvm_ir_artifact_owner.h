#ifndef PGY_SELF_HOST_LLVM_IR_ARTIFACT_OWNER_H
#define PGY_SELF_HOST_LLVM_IR_ARTIFACT_OWNER_H

#include <stdbool.h>

int driver_publish_self_host_llvm_ir_file(
    const char *launcher_path,
    const char *source_path,
    const char *output_path,
    bool emit_json_diagnostic);

#endif /* PGY_SELF_HOST_LLVM_IR_ARTIFACT_OWNER_H */
