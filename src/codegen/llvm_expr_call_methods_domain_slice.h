#ifndef PGY_LLVM_EXPR_CALL_METHODS_DOMAIN_SLICE_H
#define PGY_LLVM_EXPR_CALL_METHODS_DOMAIN_SLICE_H

#include "llvm_internal.h"

LLVMValueRef llvm_emit_member_call_slot_method(ASTNode *node,
                                               LLVMGenCtx *ctx,
                                               ASTNode *obj_node,
                                               const char *method_name);
LLVMValueRef llvm_emit_member_call_slice(ASTNode *node,
                                         LLVMGenCtx *ctx,
                                         ASTNode *obj_node,
                                         const char *method_name);

#endif
