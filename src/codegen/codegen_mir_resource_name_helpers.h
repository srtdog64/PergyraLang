#ifndef PGY_SRC_CODEGEN_CODEGEN_MIR_RESOURCE_NAME_HELPERS_H
#define PGY_SRC_CODEGEN_CODEGEN_MIR_RESOURCE_NAME_HELPERS_H

#include <stdbool.h>
#include <stddef.h>

typedef enum TranspilerMIRResourceOp {
    TRANS_MIR_RESOURCE_OP_NONE = 0,
    TRANS_MIR_RESOURCE_OP_BORROW_READ,
    TRANS_MIR_RESOURCE_OP_BORROW_WRITE,
    TRANS_MIR_RESOURCE_OP_CLAIM,
    TRANS_MIR_RESOURCE_OP_MOVE,
    TRANS_MIR_RESOURCE_OP_READ,
    TRANS_MIR_RESOURCE_OP_RELEASE,
    TRANS_MIR_RESOURCE_OP_WRITE,
} TranspilerMIRResourceOp;

const char *transpiler_extract_type_suffix_from_fn(const char *fn_name);

TranspilerMIRResourceOp transpiler_mir_resource_op_lookup(const char *op_name);

bool transpiler_format_slot_runtime_fn(const char *op_name,
                                       bool is_secure_slot,
                                       bool is_device_slot,
                                       const char *inner_name,
                                       char *out,
                                       size_t out_size);
#endif /* PGY_SRC_CODEGEN_CODEGEN_MIR_RESOURCE_NAME_HELPERS_H */
