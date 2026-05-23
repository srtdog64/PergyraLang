/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker implementation
 */

#include "type_checker_internal.h"

/* type_check_ability_decl body moved to type_checker_ability_decl.c.
 * See docs/101_semantic_split_template.md. */

bool
type_check_statement(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL)
        return true;

    switch (node->type) {
    case AST_LET_DECL:
        return type_check_let_decl(node, ctx);
    case AST_LET_DESTRUCTURE:
        return type_check_let_destructure_stmt(node, ctx);
    case AST_FUNC_DECL:
        return type_check_func_decl(node, ctx);
    case AST_EVENT_DECL:
        return type_check_event_decl(node, ctx);
    case AST_TYPE_ALIAS:
        return type_check_type_alias_stmt(node, ctx);
    case AST_CLASS_DECL:
        return type_check_class_decl(node, ctx);
    case AST_EXTERN_BLOCK:
        return type_check_extern_block(node, ctx);
    case AST_IF_STMT:
        return type_check_if_stmt(node, ctx);
    case AST_FOR_LOOP:
        return type_check_for_loop(node, ctx);
    case AST_WHILE_LOOP:
        return type_check_while_loop(node, ctx);
    case AST_MATCH_STMT:
        return type_check_match_stmt(node, ctx);
    case AST_RETURN:
        return type_check_return_stmt(node, ctx);
    case AST_BREAK:
        return type_check_break_stmt(node, ctx);
    case AST_CONTINUE:
        return type_check_continue_stmt(node, ctx);
    case AST_ENUM_DECL:
        return type_check_enum_decl(node, ctx);
    case AST_WITH_STMT:
        return type_check_with_stmt(node, ctx);
    case AST_PARALLEL_BLOCK:
        return type_check_parallel_block(node, ctx);
    case AST_ABILITY_DECL:
        return type_check_ability_decl(node, ctx);
    case AST_ROLE_DECL:
        return type_check_role_decl(node, ctx);
    case AST_PARTY_DECL:
        return type_check_party_decl(node, ctx);
    case AST_ROSTER_DECL:
        return type_check_roster_decl(node, ctx);
    case AST_WORLD_DECL:
        return type_check_world_decl(node, ctx);
    case AST_INTENT_DECL:
        return type_check_intent_decl(node, ctx);
    case AST_RELATION_DECL:
        return type_check_relation_decl(node, ctx);
    case AST_EFFECT_DECL:
        return type_check_effect_decl(node, ctx);
    case AST_ZONE_DECL:
        return type_check_zone_decl(node, ctx);
    case AST_ASYNC_BLOCK:
        return type_check_async_block(node, ctx);
    case AST_SELECT_STMT:
        return type_check_select_stmt(node, ctx);
    case AST_EVENT_SUBSCRIBE:
        return type_check_event_subscription(node, ctx, "subscription");
    case AST_EVENT_UNSUBSCRIBE:
        return type_check_event_subscription(node, ctx, "unsubscription");
    case AST_EVENT_INVOKE:
        return type_check_event_invoke_stmt(node, ctx);
    case AST_BLOCK:
        return type_check_block(node, ctx);
    case AST_IMPORT_DECL:
        /* Already resolved by driver; skip. */
        return true;
    case AST_USE_DECL:
        return type_check_use_decl(node, ctx);
    case AST_NAMESPACE_DECL:
        return type_check_namespace_decl(node, ctx);
    case AST_UNSAFE_BLOCK:
        return type_check_unsafe_block(node, ctx);
    case AST_DEFER_STMT:
        return type_check_defer_stmt(node, ctx);
    case AST_BIND_STMT:
        /* bind party.slot = Role; validated at codegen level. */
        return true;
    default:
        /* Expression statement */
        type_check_expression(node, ctx);
        return !ctx->has_error;
    }
}
