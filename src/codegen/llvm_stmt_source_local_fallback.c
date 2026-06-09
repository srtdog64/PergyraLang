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

/* P0 #4 follow-up: host-method return-type AST fallback.
 *
 * When the MIR decl-header inventory hasn't enumerated a world / class
 * method (dnd_tavern_campaign's `TavernCampaignWorld.TavernRecruitment`
 * is the motivating case), reach for the AST FuncDecl directly. Mirrors
 * the C transpiler's df9a6737 fallback. Returns the FuncDecl on hit,
 * NULL when:
 *   - the name resolves to a global callable (the caller's function-
 *     metadata path picks up; not a host method)
 *   - no nominal host-method decl exists for this host/name pair.
 *
 * Lives in this TU rather than inline in llvm_stmt_type_infer.c so the
 * type-infer TU stays under the production-c-size 900-line cap. */
ASTNode *
llvm_stmt_host_method_ast_decl(LLVMGenCtx *ctx,
                               const char *host_type_name,
                               const char *method_name)
{
    if (ctx == NULL || host_type_name == NULL || method_name == NULL)
        return NULL;
    ASTNode *callable = llvm_find_callable_decl(ctx, method_name);
    if (callable != NULL && callable->type == AST_FUNC_DECL)
        return NULL;
    ASTNode *m = llvm_find_nominal_host_method_decl(
        ctx, host_type_name, method_name);
    return (m != NULL && m->type == AST_FUNC_DECL) ? m : NULL;
}

#endif /* PGY_LLVM_ENABLED */
