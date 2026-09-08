#ifndef PGY_LLVM_EXPR_SPAWN_CALL_HELPERS_H
#define PGY_LLVM_EXPR_SPAWN_CALL_HELPERS_H

#include "llvm_internal.h"

LLVMValueRef llvm_await_task_handle(LLVMGenCtx *ctx,
                                    ASTNode *node,
                                    LLVMValueRef task,
                                    const char *inner,
                                    bool is_remote);
LLVMFuncEntry *llvm_resolve_callee_entry(LLVMGenCtx *ctx,
                                         ASTNode *call,
                                         const char *callee_name,
                                         const MIRRoutine **bound_routine);
LLVMValueRef llvm_emit_spawn_expr(ASTNode *node, LLVMGenCtx *ctx);

#endif
