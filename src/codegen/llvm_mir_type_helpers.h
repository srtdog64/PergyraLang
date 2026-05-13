#ifndef PGY_LLVM_MIR_TYPE_HELPERS_H
#define PGY_LLVM_MIR_TYPE_HELPERS_H

#include "llvm_internal.h"

LLVMTypeRef llvm_mir_type_from_abi_layout(LLVMGenCtx *ctx,
                                          const MIRTypeLayout *layout);
LLVMTypeRef llvm_mir_type_from_ast(LLVMGenCtx *ctx, ASTNode *type_node);
LLVMTypeRef llvm_mir_required_type_from_ast(LLVMGenCtx *ctx,
                                            ASTNode *owner,
                                            ASTNode *type_node,
                                            const char *slot_kind);
bool llvm_mir_param_uses_pointer_self(LLVMGenCtx *ctx, ASTNode *type_node);
const char *llvm_mir_boundary_slot_inner_name(LLVMGenCtx *ctx,
                                              FuncParam *param,
                                              bool *is_secure_out);

#endif
