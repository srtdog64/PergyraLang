/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Destructuring ownership-boundary checks.
 */

#include <string.h>

#include "diag_codes.h"
#include "type_checker_internal.h"
#include "type_checker_ownership_consumers_internal.h"
#include "type_checker_ownership_internal.h"

static Type *
ownership_destructure_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

static bool
type_check_let_destructure_tail(ASTNode *node, ASTNode *init,
                                SemanticContext *ctx)
{
    Type *init_type = init != NULL
        ? ownership_destructure_normalize_type(type_check_expression(init, ctx))
        : TYPE_UNKNOWN;

    if (type_is_tuple(init_type)) {
        size_t arity = type_tuple_arity(init_type);
        size_t binds = ast_let_destructure_name_count(node);
        if (arity != binds) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_DESTRUCTURING_ARITY_MISMATCH,
                PGY_FIX_ALIGN_DESTRUCTURING_ARITY,
                node,
                "Tuple destructuring arity mismatch: binding %llu, tuple arity %llu",
                (unsigned long long) binds, (unsigned long long) arity);
            return false;
        }
        for (size_t i = 0; i < binds; i++) {
            Type *elem = type_tuple_get_element(init_type, i);
            if (init != NULL) {
                semantic_validate_borrowed_escape(
                    node, init, ctx, init_type, NULL,
                    OWNERSHIP_CONSUMER_DESTRUCTURE_TARGET_BINDING, NULL,
                    ast_let_destructure_name(node, i), NULL,
                    false, NULL, NULL);
            }
            Symbol *s = symbol_create_variable(
                ast_let_destructure_name(node, i),
                elem != NULL ? elem : TYPE_UNKNOWN,
                node->line, node->column);
            scope_declare(ctx->scope, s);
        }
        return true;
    }

    for (size_t i = 0; i < ast_let_destructure_name_count(node); i++) {
        Type *elem_type = TYPE_UNKNOWN;
        if (type_is_constructed_named(init_type, "Array")
            || type_is_constructed_named(init_type, "Slice")) {
            elem_type = type_get_constructed_arg(init_type, 0);
        }
        if (init != NULL) {
            semantic_validate_borrowed_escape(
                node, init, ctx, init_type, NULL,
                OWNERSHIP_CONSUMER_DESTRUCTURE_TARGET_BINDING, NULL,
                ast_let_destructure_name(node, i), NULL,
                false, NULL, NULL);
        }
        Symbol *s = symbol_create_variable(
            ast_let_destructure_name(node, i), elem_type,
            node->line, node->column);
        scope_declare(ctx->scope, s);
    }

    return true;
}

bool
type_check_let_destructure_stmt(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *init;

    if (node == NULL || node->type != AST_LET_DESTRUCTURE)
        return true;

    init = ast_let_destructure_initializer(node);

    if (init != NULL
        && init->type == AST_CALL
        && ast_call_callee(init) != NULL
        && ast_call_callee(init)->type == AST_IDENTIFIER
        && ast_identifier_name(ast_call_callee(init)) != NULL
        && ast_call_generic_arg_count(init) >= 1) {
        const char *callee_name =
            ast_identifier_name(ast_call_callee(init));
        bool is_claim_slot =
            (strcmp(callee_name, "ClaimSlot") == 0);
        bool is_claim_secure =
            (strcmp(callee_name, "ClaimSecureSlot") == 0);
        if (is_claim_slot || is_claim_secure) {
            GenericParam *inner_param =
                ast_call_generic_arg(init, 0);
            const char *inner_name =
                ast_generic_param_name(inner_param);
            ASTNode *inner_node =
                ast_generic_param_constraint(inner_param);
            Type *inner_type = NULL;
            if (inner_node != NULL)
                inner_type = domain_resolve_type_ref(inner_node, ctx);
            if (inner_type == NULL && inner_name != NULL) {
                ASTNode *synth = ast_create_type(inner_name);
                if (synth != NULL) {
                    inner_type = domain_resolve_type_ref(synth, ctx);
                    ast_destroy(synth);
                }
            }
            if (inner_type == NULL)
                inner_type = TYPE_UNKNOWN;

            Type *slot_type = type_create_slot(inner_type, is_claim_secure);
            if (is_claim_secure)
                semantic_record_effect(ctx, EFFECT_SECURE);

            if (is_claim_secure && ast_let_destructure_name_count(node) == 2) {
                const char *slot_name =
                    ast_let_destructure_name(node, 0);
                const char *token_name =
                    ast_let_destructure_name(node, 1);
                Symbol *slot_sym = symbol_create_slot(slot_name, slot_type,
                    true, token_name, node->line, node->column);
                scope_declare(ctx->scope, slot_sym);
                Symbol *tok_sym = symbol_create_token(token_name,
                    slot_name, node->line, node->column);
                if (tok_sym != NULL) {
                    Type *token_args[1] = { inner_type };
                    tok_sym->type = type_create_constructed(TYPE_TOKEN,
                        token_args, 1);
                }
                if (!scope_declare(ctx->scope, tok_sym))
                    symbol_destroy(tok_sym);
                scope_register_slot(ctx->scope, slot_sym);
                return true;
            }
            if (is_claim_slot && ast_let_destructure_name_count(node) == 1) {
                const char *slot_name =
                    ast_let_destructure_name(node, 0);
                Symbol *slot_sym = symbol_create_slot(slot_name, slot_type,
                    false, NULL, node->line, node->column);
                scope_declare(ctx->scope, slot_sym);
                scope_register_slot(ctx->scope, slot_sym);
                return true;
            }
        }
    }

    return type_check_let_destructure_tail(node, init, ctx);
}
