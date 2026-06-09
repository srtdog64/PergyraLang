/*
 * Copyright (c) 2026 Pergyra Language Project
 * Last-resort AST source-local class lookup for LLVM type inference.
 *
 * Split out of llvm_stmt_type_infer.c (P0 #4) so type_infer stays
 * under the production-c-size cap. The MEMBER_ACCESS field branch
 * and the MEMBER_ACCESS call-receiver branch both consult the same
 * walk: when every var-class registry / field-class / zone-slot /
 * custom-type-name probe misses, fall back to walking the current
 * function body for a matching `let receiver: ClassName = ...`
 * annotation.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "../compiler/mir.h"
#include "../parser/ast_api.h"

LLVMClassTypeEntry *
llvm_stmt_source_local_class(LLVMGenCtx *ctx, ASTNode *recv)
{
    if (ctx == NULL || recv == NULL
        || recv->type != AST_IDENTIFIER
        || ast_identifier_name(recv) == NULL
        || ctx->current_func_decl == NULL
        || ctx->current_func_decl->type != AST_FUNC_DECL) {
        return NULL;
    }
    const char *ann = mir_source_local_type_name_in_ast(
        ast_func_body(ctx->current_func_decl),
        ast_identifier_name(recv));
    if (ann == NULL)
        return NULL;
    return llvm_lookup_class(ctx, ann);
}

#endif /* PGY_LLVM_ENABLED */
