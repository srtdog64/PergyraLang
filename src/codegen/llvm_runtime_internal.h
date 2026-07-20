#ifndef PERGYRA_LLVM_RUNTIME_INTERNAL_H
#define PERGYRA_LLVM_RUNTIME_INTERNAL_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "../compiler/mir_abi_layout.h"

void llvm_declare_runtime_raw_collections(LLVMGenCtx *ctx);
void llvm_declare_runtime_core_builtins(LLVMGenCtx *ctx);
void llvm_declare_runtime_channels(LLVMGenCtx *ctx);
void llvm_declare_runtime_secure_slots(LLVMGenCtx *ctx);
void llvm_declare_runtime_task_memory(LLVMGenCtx *ctx);

/*
 * The MIR ABI row is the only owner of slot runtime spelling and call shape.
 * A missing row is deliberately returned as NULL so callers can retain the
 * explicit nominal/structural compatibility boundary; a present row with a
 * mismatched shape is a hard LLVM error.
 */
const MIRResourceRuntimeRow *llvm_slot_runtime_row_for_operation(
    ASTNode *node,
    LLVMGenCtx *ctx,
    MIRResourceAbiKind kind,
    const char *inner_type_name,
    const char *operation);

#endif /* PGY_LLVM_ENABLED */

#endif /* PERGYRA_LLVM_RUNTIME_INTERNAL_H */
