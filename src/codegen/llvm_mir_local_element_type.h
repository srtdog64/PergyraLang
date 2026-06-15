#ifndef PGY_LLVM_MIR_LOCAL_ELEMENT_TYPE_H
#define PGY_LLVM_MIR_LOCAL_ELEMENT_TYPE_H

#include "llvm_internal.h"

LLVMTypeRef llvm_mir_local_elem_type_from_type_name(
    LLVMGenCtx *ctx,
    const char *type_name);
LLVMTypeRef llvm_mir_local_elem_type_from_layout(
    LLVMGenCtx *ctx,
    const MIRTypeLayout *layout);
LLVMTypeRef llvm_mir_local_elem_type_from_type_ast(
    LLVMGenCtx *ctx,
    ASTNode *type_node);
bool llvm_mir_local_require_elem_type(
    LLVMGenCtx *ctx,
    ASTNode *site,
    LLVMTypeRef elem_type,
    const char *surface);

#endif /* PGY_LLVM_MIR_LOCAL_ELEMENT_TYPE_H */
