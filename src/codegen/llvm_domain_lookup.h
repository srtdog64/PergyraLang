/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM domain declaration lookup policy.
 */

#ifndef PERGYRA_LLVM_DOMAIN_LOOKUP_H
#define PERGYRA_LLVM_DOMAIN_LOOKUP_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

bool llvm_named_domain_decl_exists(LLVMGenCtx *ctx, ASTNodeType decl_type,
                                   const char *name);
bool llvm_domain_constructor_decl_exists(LLVMGenCtx *ctx, const char *name);
bool llvm_function_decl_exists(LLVMGenCtx *ctx, const char *name);
bool llvm_intent_decl_exists(LLVMGenCtx *ctx, const char *name);
bool llvm_callable_decl_exists(LLVMGenCtx *ctx, const char *name);
bool llvm_projection_nominal_decl_exists(LLVMGenCtx *ctx, const char *name);

#endif /* PGY_LLVM_ENABLED */

#endif /* PERGYRA_LLVM_DOMAIN_LOOKUP_H */
