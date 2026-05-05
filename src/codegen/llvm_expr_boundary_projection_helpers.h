#ifndef PGY_LLVM_EXPR_BOUNDARY_PROJECTION_HELPERS_H
#define PGY_LLVM_EXPR_BOUNDARY_PROJECTION_HELPERS_H

#include "llvm_internal.h"
#include "llvm_boundary_slot_param.h"

ASTNode *llvm_find_function_decl(LLVMGenCtx *ctx, const char *name);

ASTNode *llvm_find_intent_decl(LLVMGenCtx *ctx, const char *name);

LLVMValueRef *llvm_build_boundary_call_args(LLVMGenCtx *ctx, ASTNode *decl,
                                            ASTNode **arg_nodes, size_t argc,
                                            unsigned *out_count);

void llvm_append_mangled_suffix(char *buf, size_t buf_size,
                                const char *suffix);

#include "llvm_expr_projection_path_helpers.h"

#endif
