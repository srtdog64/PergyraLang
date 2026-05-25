#include "type_checker_internal.h"

#include <string.h>

static Type *
intent_binding_context_resolve_binding_type(ASTNode *intent,
                                            const char *alias,
                                            SemanticContext *ctx)
{
    ASTNode *involves = find_intent_involves_local(intent, alias);
    ASTNode *value;

    if (involves != NULL)
        return intent_normalize_type(intent_resolve_involves_type(involves, ctx));

    value = find_intent_value_local(intent, alias);
    return intent_normalize_type(intent_resolve_value_type(value, ctx));
}

static const char *
find_unique_intent_binding_alias_by_type_name(ASTNode *intent,
                                              const char *type_name,
                                              SemanticContext *ctx)
{
    const char *matched_alias = NULL;
    ASTNode **involves_nodes;
    size_t involve_count;
    ASTNode **values;
    size_t value_count;

    if (intent == NULL || intent->type != AST_INTENT_DECL || type_name == NULL)
        return NULL;

    involves_nodes = ast_intent_decl_involves(intent, &involve_count);
    for (size_t i = 0; i < involve_count; i++) {
        ASTNode *involves = involves_nodes[i];
        Type *participant_type = intent_resolve_involves_type(involves, ctx);

        if (participant_type == NULL || participant_type->name == NULL
            || strcmp(participant_type->name, type_name) != 0) {
            continue;
        }
        if (matched_alias != NULL)
            return NULL;
        matched_alias = ast_intent_involves_alias(involves);
    }

    values = ast_intent_decl_values(intent, &value_count);
    for (size_t i = 0; i < value_count; i++) {
        ASTNode *value = values[i];
        Type *value_type = intent_resolve_value_type(value, ctx);

        if (value_type == NULL || value_type->name == NULL
            || strcmp(value_type->name, type_name) != 0) {
            continue;
        }
        if (matched_alias != NULL)
            return NULL;
        matched_alias = ast_intent_value_alias(value);
    }

    return matched_alias;
}

void
intent_step_derive_transfer_context(ASTNode *intent_decl,
                                    ASTNode *step,
                                    SemanticContext *ctx)
{
    const char *to_alias = NULL;
    ASTNode *to_involves;
    Type *to_type;

    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP)
        return;

    to_involves = intent_step_resolve_transfer_target_involves(
        intent_decl, step, &to_alias);
    if (to_involves == NULL || to_involves->type != AST_INTENT_INVOLVES)
        return;

    to_type = intent_resolve_involves_type(to_involves, ctx);

    if (ast_intent_step_using_expr(step) == NULL
        && ast_intent_step_set_using_expr(step, ast_create_identifier(to_alias))) {
        ast_intent_step_mark_derived_using_from_transfer(step);
    }

    if (ast_intent_step_where_type(step) == NULL
        && to_type != NULL
        && to_type->name != NULL) {
        (void)intent_step_set_where_type_name(
            step, to_type->name,
            INTENT_STEP_WHERE_PROVENANCE_DERIVED_TRANSFER);
    }
}

void
intent_step_derive_zone_binding_context(ASTNode *intent_decl,
                                        ASTNode *step,
                                        SemanticContext *ctx)
{
    ASTNode *using_expr;
    ASTNode *where_type;

    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP) {
        return;
    }
    using_expr = ast_intent_step_using_expr(step);
    where_type = ast_intent_step_where_type(step);

    if (where_type == NULL
        && using_expr != NULL
        && using_expr->type == AST_IDENTIFIER) {
        Type *using_type = intent_binding_context_resolve_binding_type(
            intent_decl, ast_identifier_name(using_expr), ctx);
        ASTNode *zone_decl = intent_find_zone_decl_for_type(using_type, ctx);
        if (zone_decl != NULL && using_type != NULL && using_type->name != NULL) {
            (void)intent_step_set_where_type_name(
                step, using_type->name,
                INTENT_STEP_WHERE_PROVENANCE_DERIVED_USING);
        }
    }

    using_expr = ast_intent_step_using_expr(step);
    where_type = ast_intent_step_where_type(step);
    if (using_expr == NULL && where_type != NULL) {
        Type *zone_type = intent_normalize_type(
            intent_resolve_type_ref(where_type, ctx));

        if (zone_type == NULL || zone_type->name == NULL)
            return;

        const char *matched_alias = find_unique_intent_binding_alias_by_type_name(
            intent_decl, zone_type->name, ctx);
        if (matched_alias != NULL) {
            if (ast_intent_step_set_using_expr(step,
                    ast_create_identifier(matched_alias))) {
                ast_intent_step_mark_derived_using_from_where(step);
            }
        }
    }
}
