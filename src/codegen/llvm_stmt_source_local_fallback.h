/*
 * Copyright (c) 2026 Pergyra Language Project
 * P0 #4 source-local MIR owner and non-MIR compatibility declarations.
 */
#ifndef PGY_LLVM_STMT_SOURCE_LOCAL_FALLBACK_H
#define PGY_LLVM_STMT_SOURCE_LOCAL_FALLBACK_H
#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
LLVMClassTypeEntry *llvm_stmt_source_local_class(LLVMGenCtx *ctx, ASTNode *recv);
ASTNode *llvm_stmt_source_local_let_init(LLVMGenCtx *ctx, const char *name);
ASTNode *llvm_stmt_host_method_ast_decl(LLVMGenCtx *ctx,
                                        const char *host_type_name,
                                        const char *method_name);
#endif
#endif
