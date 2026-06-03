#ifndef PGY_LLVM_MEMBER_CALL_INTERNAL_H
#define PGY_LLVM_MEMBER_CALL_INTERNAL_H

#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

LLVMValueRef llvm_member_call_error_recovery(LLVMGenCtx *ctx,
                                             ASTNode *node,
                                             const char *class_name,
                                             const char *method_name,
                                             const char *message);
LLVMValueRef *llvm_member_call_alloc_args(LLVMGenCtx *ctx,
                                          ASTNode *node,
                                          const char *class_name,
                                          const char *method_name,
                                          size_t argc);
bool llvm_member_call_store_arg(LLVMGenCtx *ctx,
                                ASTNode *node,
                                const char *class_name,
                                const char *method_name,
                                LLVMValueRef *args,
                                size_t index,
                                LLVMValueRef value);
LLVMValueRef llvm_member_call_adjust_pointer_self_arg(
    LLVMGenCtx *ctx,
    const MIRDeclMethod *method_meta,
    ASTNode *method_decl,
    size_t logical_index,
    ASTNode *arg_node,
    LLVMValueRef arg_val);
char *llvm_member_call_mangle_method_name(LLVMGenCtx *ctx,
                                          ASTNode *node,
                                          const char *class_name,
                                          const char *method_name);

#endif

#endif
