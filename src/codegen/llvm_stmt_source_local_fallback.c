/*
 * Copyright (c) 2026 Pergyra Language Project
 * MIR-owned source-local class lookup for LLVM type inference.
 *
 * Split out of llvm_stmt_type_infer.c (P0 #4) so type_infer stays
 * under the production-c-size cap. The MEMBER_ACCESS field branch
 * and the MEMBER_ACCESS call-receiver branch both consult the same
 * lookup: when every var-class registry / field-class / zone-slot /
 * custom-type-name probe misses, query the active MIR routine owner
 * for a matching `let receiver: ClassName = ...` annotation. Only
 * legacy non-MIR callers fall back to walking the current AST body.
 */

#ifdef PGY_LLVM_ENABLED

#include <string.h>

#include "llvm_internal.h"
#include "../compiler/mir.h"
#include "../compiler/mir_source_local_types.h"
#include "../parser/ast_api.h"

/* Locate the initializer of `let <name> = <init>` within a statement subtree.
 * Used to recover the type of an unannotated local during the type-inference
 * pre-pass, when the variable has no scope alloca and no source-local-type
 * fact (the fact capture only records explicit annotations). */
static ASTNode *
llvm_find_let_init_in_node(ASTNode *node, const char *name)
{
    if (node == NULL || name == NULL)
        return NULL;
    switch (node->type) {
    case AST_LET_DECL:
        /* Only unannotated lets need initializer-based recovery; an annotated
         * let (e.g. a function type) is resolved through its annotation, and
         * inferring its initializer here can recurse into unsupported forms. */
        if (ast_let_name(node) != NULL
            && strcmp(ast_let_name(node), name) == 0
            && ast_let_type(node) == NULL)
            return ast_let_initializer(node);
        return NULL;
    case AST_BLOCK: {
        size_t n = ast_block_statement_count(node);
        for (size_t i = 0; i < n; i++) {
            ASTNode *hit = llvm_find_let_init_in_node(
                ast_block_statement(node, i), name);
            if (hit != NULL)
                return hit;
        }
        return NULL;
    }
    case AST_IF_STMT: {
        ASTNode *hit = llvm_find_let_init_in_node(
            ast_if_then_branch(node), name);
        if (hit != NULL)
            return hit;
        return llvm_find_let_init_in_node(ast_if_else_branch(node), name);
    }
    case AST_WHILE_LOOP:
        return llvm_find_let_init_in_node(ast_while_body(node), name);
    case AST_FOR_LOOP:
        return llvm_find_let_init_in_node(ast_for_body(node), name);
    case AST_WITH_STMT:
        return llvm_find_let_init_in_node(ast_with_body(node), name);
    default:
        return NULL;
    }
}

ASTNode *
llvm_stmt_non_mir_source_local_let_init(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL
        || llvm_active_has_mir(ctx)
        || ctx->current_func_decl == NULL
        || ctx->current_func_decl->type != AST_FUNC_DECL)
        return NULL;
    return llvm_find_let_init_in_node(
        ast_func_body(ctx->current_func_decl), name);
}

LLVMTypeRef
llvm_stmt_source_local_type(LLVMGenCtx *ctx, const char *name)
{
    const MIRRoutine *routine;
    const char *type_name;

    if (ctx == NULL || name == NULL)
        return NULL;
    routine = ctx->current_mir_routine;
    if (routine == NULL)
        routine = llvm_active_function_routine_by_name(
            ctx, ctx->current_func_decl != NULL
                && ctx->current_func_decl->type == AST_FUNC_DECL
                    ? ast_declaration_name(ctx->current_func_decl)
                    : NULL);
    if (routine == NULL && llvm_active_has_mir(ctx))
        return NULL;
    type_name = routine != NULL
        ? mir_routine_source_local_type_name(routine, name)
        : mir_source_local_type_name_in_ast(
            ast_func_body(ctx->current_func_decl), name);
    return type_name != NULL ? pergyra_type_to_llvm(ctx, type_name) : NULL;
}

LLVMClassTypeEntry *
llvm_stmt_source_local_class(LLVMGenCtx *ctx, ASTNode *recv)
{
    if (ctx == NULL || recv == NULL
        || recv->type != AST_IDENTIFIER
        || ast_identifier_name(recv) == NULL) {
        return NULL;
    }
    const MIRRoutine *routine = ctx->current_mir_routine;
    if (routine == NULL) {
        routine = llvm_active_function_routine_by_name(
            ctx, ctx->current_func_decl != NULL
                && ctx->current_func_decl->type == AST_FUNC_DECL
                    ? ast_declaration_name(ctx->current_func_decl)
                    : NULL);
    }
    if (routine == NULL && llvm_active_has_mir(ctx))
        return NULL;
    const char *ann = routine != NULL
        ? mir_routine_source_local_type_name(routine, ast_identifier_name(recv))
        : mir_source_local_type_name_in_ast(ast_func_body(ctx->current_func_decl),
            ast_identifier_name(recv));
    if (ann == NULL)
        return NULL;
    return llvm_lookup_class(ctx, ann);
}

#endif /* PGY_LLVM_ENABLED */
