#ifndef PGY_LLVM_EXPR_BOUNDARY_PROJECTION_HELPERS_H
#define PGY_LLVM_EXPR_BOUNDARY_PROJECTION_HELPERS_H

#include "llvm_internal.h"

LLVMValueRef *llvm_build_boundary_call_args(LLVMGenCtx *ctx, ASTNode *decl,
                                            ASTNode **arg_nodes, size_t argc,
                                            unsigned *out_count);

#endif
