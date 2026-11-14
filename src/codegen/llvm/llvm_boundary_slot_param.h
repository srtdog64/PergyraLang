#ifndef PGY_LLVM_BOUNDARY_SLOT_PARAM_H
#define PGY_LLVM_BOUNDARY_SLOT_PARAM_H

#include "llvm_internal.h"

const char *llvm_boundary_slot_inner_name(LLVMGenCtx *ctx, FuncParam *param,
                                          bool *is_secure_out);
const char *llvm_boundary_slot_inner_name_from_type_name(
    LLVMGenCtx *ctx,
    FuncParam *param,
    const char *type_name,
    bool *is_secure_out);
const char *llvm_mir_boundary_resource_inner_name(
    LLVMGenCtx *ctx,
    const MIRRoutine *routine,
    size_t param_index,
    MIRParamResourceKind *resource_kind_out);

#endif
