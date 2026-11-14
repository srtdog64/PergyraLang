#ifndef PGY_LLVM_EXPR_CALL_METHODS_VTABLE_DISPATCH_H
#define PGY_LLVM_EXPR_CALL_METHODS_VTABLE_DISPATCH_H

#include "llvm_internal.h"

LLVMValueRef llvm_emit_member_call_vtable_dispatch(ASTNode *node,
                                                   LLVMGenCtx *ctx,
                                                   ASTNode *obj_node,
                                                   const char *method_name);

#endif
