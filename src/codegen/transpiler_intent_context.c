/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend intent context and binding lookup helpers.
 */

#include <string.h>

#include "transpiler_intent_context.h"
#include "transpiler_decl_lookup.h"

ASTNode *
find_intent_participant_local(ASTNode *intent, const char *alias)
{
    ASTNode **involves_nodes;
    size_t involve_count;

    if (intent == NULL || intent->type != AST_INTENT_DECL || alias == NULL)
        return NULL;
    involves_nodes = ast_intent_decl_involves(intent, &involve_count);
    for (size_t i = 0; i < involve_count; i++) {
        ASTNode *involves = involves_nodes[i];
        if (involves != NULL && involves->type == AST_INTENT_INVOLVES
            && ast_intent_involves_alias(involves) != NULL
            && strcmp(ast_intent_involves_alias(involves), alias) == 0) {
            return involves;
        }
    }
    return NULL;
}

ASTNode *
find_subject_action_decl(TranspilerCtx *ctx,
                         const char *subject_name,
                         const char *action_name)
{
    ASTNode *decl;
    ASTNode *method;

    if (ctx == NULL || subject_name == NULL || action_name == NULL)
        return NULL;

    decl = find_subject_host_decl(ctx, subject_name);
    if (decl == NULL || decl->type != AST_CLASS_DECL
        || ast_class_nominal_kind(decl) != NOMINAL_DECL_SUBJECT) {
        return NULL;
    }

    method = find_nominal_host_method_decl(ctx, subject_name, action_name);
    if (method == NULL || method->type != AST_FUNC_DECL
        || !method->data.func_decl.is_action) {
        return NULL;
    }
    return method;
}

ASTNode *
find_zone_decl_in_program_view(TranspilerCtx *ctx, const char *zone_name)
{
    return transpiler_find_decl_in_inventory_local(ctx, AST_ZONE_DECL,
                                                   zone_name);
}

const char *
intent_participant_type_name(ASTNode *intent, const char *alias)
{
    ASTNode *involves = find_intent_participant_local(intent, alias);
    ASTNode *subject_type = ast_intent_involves_subject_type(involves);
    if (involves != NULL
        && subject_type != NULL
        && subject_type->type == AST_TYPE) {
        return subject_type->data.type.name;
    }
    return NULL;
}

const char *
intent_step_effective_zone_alias(ASTNode *step)
{
    if (step == NULL || step->type != AST_INTENT_STEP)
        return NULL;
    if (ast_intent_step_using_expr(step) != NULL
        && ast_intent_step_using_expr(step)->type == AST_IDENTIFIER) {
        return ast_intent_step_using_expr(step)->data.identifier.name;
    }
    return ast_intent_step_transfer_to_alias(step);
}

const char *
intent_zone_binding_type_name(ASTNode *intent, const char *alias)
{
    ASTNode *involves = find_intent_participant_local(intent, alias);
    ASTNode *subject_type = ast_intent_involves_subject_type(involves);
    if (involves != NULL
        && subject_type != NULL
        && subject_type->type == AST_TYPE) {
        return subject_type->data.type.name;
    }
    return NULL;
}

const char *
intent_zone_binding_type_name_with_metadata(ASTNode *intent,
                                            const char *alias,
                                            const char **participant_aliases,
                                            const char **participant_types,
                                            size_t participant_count)
{
    if (alias != NULL && participant_aliases != NULL && participant_types != NULL) {
        for (size_t i = 0; i < participant_count; i++) {
            if (participant_aliases[i] != NULL
                && participant_types[i] != NULL
                && strcmp(participant_aliases[i], alias) == 0) {
                return participant_types[i];
            }
        }
    }
    return intent_zone_binding_type_name(intent, alias);
}
