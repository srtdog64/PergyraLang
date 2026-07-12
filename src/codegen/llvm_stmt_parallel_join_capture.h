#ifndef PGY_LLVM_STMT_PARALLEL_JOIN_CAPTURE_H
#define PGY_LLVM_STMT_PARALLEL_JOIN_CAPTURE_H

#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "../compiler/mir_parallel_capture_facts.h"

#define PJOIN_MAX_CAPTURES 64

typedef struct {
    const char *name;
    LLVMValueRef alloca;
    LLVMTypeRef type;
    const char *channel_inner;
    const char *future_inner;
    const char *slot_inner;
    bool future_is_remote;
    bool slot_is_secure;
    bool is_admitted_array;
    LLVMTypeRef array_elem_type;
    const char *array_elem_name;
} LLVMParallelJoinCapture;

void llvm_parallel_join_set_error(LLVMGenCtx *ctx, ASTNode *node,
                                  const char *fmt, const char *arg);
bool llvm_parallel_join_collect_captures(
    LLVMGenCtx *ctx,
    ASTNode *node,
    ASTNode *body,
    const char *collection_name,
    const MIRParallelCaptureBoundaryFact *capture_boundary,
    LLVMParallelJoinCapture captures[PJOIN_MAX_CAPTURES],
    size_t *capture_count_out);
bool llvm_parallel_join_emit_alias_guard(
    LLVMGenCtx *ctx,
    ASTNode *node,
    const MIRParallelCaptureBoundaryFact *capture_boundary,
    const LLVMParallelJoinCapture captures[PJOIN_MAX_CAPTURES],
    size_t capture_count);

#endif /* PGY_LLVM_ENABLED */
#endif /* PGY_LLVM_STMT_PARALLEL_JOIN_CAPTURE_H */
