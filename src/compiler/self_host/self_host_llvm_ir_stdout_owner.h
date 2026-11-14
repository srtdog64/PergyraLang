#ifndef PGY_SELF_HOST_LLVM_IR_STDOUT_OWNER_H
#define PGY_SELF_HOST_LLVM_IR_STDOUT_OWNER_H

#include <stdbool.h>

int driver_write_self_host_llvm_ir_stdout(
    const char *launcher_path,
    const char *source_path,
    bool emit_json_diagnostic);

#endif /* PGY_SELF_HOST_LLVM_IR_STDOUT_OWNER_H */
