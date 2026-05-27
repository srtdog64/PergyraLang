/*
 * Copyright (c) 2025 Pergyra Language Project
 * Internal helpers shared by LLVM backend type-map owners.
 */

#ifndef PERGYRA_LLVM_BACKEND_TYPE_MAP_INTERNAL_H
#define PERGYRA_LLVM_BACKEND_TYPE_MAP_INTERNAL_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

LLVMTypeRef pergyra_type_to_llvm(LLVMGenCtx *ctx, const char *type_name);

char *llvm_render_type_name(ASTNode *type_node);
char *llvm_render_type_name_scratch(ASTNode *type_node, PgyArena *arena);
char *llvm_render_type_name_in_ctx(LLVMGenCtx *ctx, ASTNode *type_node);
char *llvm_render_type_name_scratch_in_ctx(LLVMGenCtx *ctx, ASTNode *type_node,
                                           PgyArena *arena);

#endif /* PGY_LLVM_ENABLED */

#endif /* PERGYRA_LLVM_BACKEND_TYPE_MAP_INTERNAL_H */
