#ifndef PERGYRA_CODEGEN_LLVM_STMT_PARALLEL_JOIN_CHUNK_H
#define PERGYRA_CODEGEN_LLVM_STMT_PARALLEL_JOIN_CHUNK_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

typedef struct {
    LLVMFuncEntry *spawn_fn;
    LLVMFuncEntry *free_fn;
    LLVMValueRef count;
    LLVMValueRef storage;
    LLVMValueRef allocation_count;
} LLVMParallelJoinChunkPlan;

typedef struct {
    LLVMValueRef chunk_wrapper_fn;
    LLVMValueRef item_contexts;
    LLVMTypeRef item_context_type;
    LLVMValueRef handles;
    LLVMTypeRef handle_type;
    LLVMValueRef loop_slot;
    LLVMValueRef item_count;
    LLVMValueRef zero;
    LLVMValueRef one;
} LLVMParallelJoinChunkFanoutInput;

LLVMValueRef llvm_parallel_join_chunk_wrapper_create(
    ASTNode *site,
    LLVMGenCtx *ctx,
    LLVMValueRef item_wrapper_fn);

bool llvm_parallel_join_chunk_plan_init(
    ASTNode *site,
    LLVMGenCtx *ctx,
    LLVMValueRef item_count,
    LLVMValueRef zero,
    LLVMValueRef one,
    LLVMParallelJoinChunkPlan *plan);
bool llvm_parallel_join_chunk_fanout(
    ASTNode *site,
    LLVMGenCtx *ctx,
    const LLVMParallelJoinChunkPlan *plan,
    const LLVMParallelJoinChunkFanoutInput *input);
void llvm_parallel_join_chunk_plan_dispose(
    LLVMGenCtx *ctx,
    const LLVMParallelJoinChunkPlan *plan);

#endif
#endif
