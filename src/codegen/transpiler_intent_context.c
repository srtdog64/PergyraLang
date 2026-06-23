/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend intent context and binding lookup helpers.
 */

#include <string.h>

#include "../compiler/mir_decl_headers.h"
#include "transpiler_intent_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_inventory_view.h"

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

const MIRDeclMethod *
find_subject_action_metadata(TranspilerCtx *ctx,
                             const char *subject_name,
                             const char *action_name)
{
    const MIRDeclHeader *header;
    ASTNode *decl;
    const MIRDeclMethod *method;

    if (ctx == NULL || subject_name == NULL || action_name == NULL)
        return NULL;

    header = transpiler_active_decl_header_of_type(
        ctx, AST_CLASS_DECL, subject_name);
    if (header != NULL) {
        if (mir_decl_header_nominal_kind_or(
                header, NOMINAL_DECL_CLASS) != NOMINAL_DECL_SUBJECT) {
            return NULL;
        }
    } else {
        if (transpiler_active_has_mir(ctx))
            return NULL;
        decl = find_subject_host_decl(ctx, subject_name);
        if (decl == NULL || decl->type != AST_CLASS_DECL
            || ast_class_nominal_kind(decl) != NOMINAL_DECL_SUBJECT) {
            return NULL;
        }
    }

    method = transpiler_find_host_method_metadata_in_context(
        ctx, subject_name, action_name);
    if (method == NULL || !transpiler_mir_decl_method_is_action_like(method))
        return NULL;
    return method;
}

bool
intent_action_metadata_has_only_self(const MIRDeclMethod *method)
{
    size_t real_pc = 0;

    if (method == NULL)
        return false;

    for (size_t i = 0; i < transpiler_mir_decl_method_param_count(method); i++) {
        FuncParam *p = transpiler_mir_decl_method_param(method, i);
        if (p == NULL || p->name == NULL)
            continue;
        if (p->type == NULL && strcmp(p->name, "self") == 0)
            continue;
        real_pc++;
    }
    return real_pc == 0;
}

ASTNode *
find_zone_decl_in_program_view(TranspilerCtx *ctx, const char *zone_name)
{
    return transpiler_find_named_decl_local(ctx, AST_ZONE_DECL, zone_name);
}

const char *
intent_participant_type_name(ASTNode *intent, const char *alias)
{
    ASTNode *involves = find_intent_participant_local(intent, alias);
    ASTNode *subject_type = NULL;

    if (involves != NULL && involves->type == AST_INTENT_INVOLVES)
        subject_type = ast_intent_involves_subject_type(involves);
    if (subject_type != NULL
        && subject_type->type == AST_TYPE) {
        return ast_type_name(subject_type);
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
        return ast_identifier_name(ast_intent_step_using_expr(step));
    }
    return ast_intent_step_transfer_to_alias(step);
}

const char *
intent_zone_binding_type_name(ASTNode *intent, const char *alias)
{
    ASTNode *involves = find_intent_participant_local(intent, alias);
    ASTNode *subject_type = NULL;

    if (involves != NULL && involves->type == AST_INTENT_INVOLVES)
        subject_type = ast_intent_involves_subject_type(involves);
    if (subject_type != NULL
        && subject_type->type == AST_TYPE) {
        return ast_type_name(subject_type);
    }
    return NULL;
}

const char *
intent_zone_binding_type_name_with_bindings(
    ASTNode *intent,
    const char *alias,
    const IntentBindingMetadataView *bindings)
{
    if (intent_binding_metadata_view_is_active(bindings))
        return intent_binding_type_name_from_metadata(
            bindings, alias, "participant");
    return intent_zone_binding_type_name(intent, alias);
}
