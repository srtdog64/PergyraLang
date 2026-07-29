/*
 * Copyright (c) 2025 Pergyra Language Project
 * Internal constructor-call owner boundaries.
 */

#ifndef PGY_LLVM_EXPR_CONSTRUCTOR_INTERNAL_H
#define PGY_LLVM_EXPR_CONSTRUCTOR_INTERNAL_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

LLVMValueRef llvm_constructor_error(ASTNode *node,
                                    LLVMGenCtx *ctx,
                                    const char *message);
LLVMValueRef llvm_emit_enum_variant_constructor(ASTNode *node,
                                                LLVMGenCtx *ctx,
                                                const char *callee_name);

#endif /* PGY_LLVM_ENABLED */

#endif /* PGY_LLVM_EXPR_CONSTRUCTOR_INTERNAL_H */
