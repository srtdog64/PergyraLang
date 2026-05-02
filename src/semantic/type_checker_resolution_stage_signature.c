#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "type_checker_internal.h"

static char *
tc_stage_signature_strdup_fmt(const char *fmt, ...)
{
    va_list ap, ap2;
    int len;
    char *buf;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (len < 0) {
        va_end(ap2);
        return NULL;
    }

    buf = malloc((size_t)len + 1);
    if (buf != NULL)
        vsnprintf(buf, (size_t)len + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

Type *
semantic_stage_resolve_type_quiet(ASTNode *type_node,
                                  SemanticContext *ctx,
                                  const ASTNode *consumer_site,
                                  const char *consumer_name,
                                  const char *reason)
{
    Type *resolved;

    if (type_node == NULL || ctx == NULL)
        return TYPE_UNKNOWN;

    if (semantic_stage_should_defer_to_graph(type_node,
                                             ctx,
                                             consumer_site,
                                             consumer_name,
                                             reason)) {
        ctx->type_resolution_stage_graph_backed_skip_count++;
        return TYPE_UNKNOWN;
    }

    resolved = semantic_type_resolution_lookup_metadata_type_ref(ctx, type_node);
    if (resolved != NULL) {
        semantic_type_resolution_record_resolved_type(ctx, type_node, resolved);
        return resolved;
    }

    return TYPE_UNKNOWN;
}

ASTNode *
semantic_stage_named_decl_quiet(SemanticContext *ctx,
                                ASTNodeType decl_type,
                                const char *provider_name)
{
    if (ctx == NULL || ctx->program_root == NULL
        || provider_name == NULL || provider_name[0] == '\0') {
        return NULL;
    }

    switch (decl_type) {
    case AST_CLASS_DECL:
        return find_type_decl_by_name(ctx->program_root, provider_name);
    case AST_ABILITY_DECL:
        return find_ability_decl_by_name(ctx->program_root, provider_name);
    case AST_ROLE_DECL:
        return semantic_find_top_level_decl_by_label(ctx->program_root,
                                                     provider_name,
                                                     TYPE_RES_NODE_DECL);
    case AST_PARTY_DECL:
    case AST_ROSTER_DECL:
    case AST_WORLD_DECL:
    case AST_ZONE_DECL:
    case AST_RELATION_DECL:
    case AST_EFFECT_DECL:
        return find_domain_decl_by_name(ctx->program_root, decl_type, provider_name);
    default:
        return semantic_find_top_level_decl_by_label(ctx->program_root,
                                                     provider_name,
                                                     TYPE_RES_NODE_DECL);
    }
}

void
semantic_stage_required_abilities(ASTNode **ability_refs,
                                  size_t ability_count,
                                  SemanticContext *ctx,
                                  const ASTNode *owner,
                                  const char *consumer_name,
                                  const char *reason)
{
    if (ability_refs == NULL || ctx == NULL)
        return;

    for (size_t i = 0; i < ability_count; i++) {
        ASTNode *ability_ref = ability_refs[i];

        if (ability_ref != NULL
            && ability_ref->type == AST_TYPE
            && ability_ref->data.type.name != NULL) {
            ctx->type_resolution_dag_ability_evidence_count++;
            (void)semantic_stage_named_decl_quiet(
                ctx,
                AST_ABILITY_DECL,
                ability_ref->data.type.name);
        }

        (void)semantic_stage_resolve_type_quiet(
            ability_ref,
            ctx,
            owner,
            consumer_name,
            reason);
    }
}

void
semantic_stage_generic_contract_nodes(GenericParams *gp,
                                      WhereClause *wc,
                                      SemanticContext *ctx,
                                      ASTNode *owner,
                                      const char *kind_name,
                                      const char *owner_name)
{
    if (ctx == NULL)
        return;

    if (gp != NULL) {
        for (size_t i = 0; i < gp->count; i++) {
            GenericParam *param = gp->params[i];
            char *consumer_name;

            if (param == NULL)
                continue;

            consumer_name = tc_stage_signature_strdup_fmt(
                "%s %s.%s",
                kind_name != NULL ? kind_name : "decl",
                owner_name != NULL ? owner_name : "<anon>",
                param->name != NULL ? param->name : "<type-param>");
            if (consumer_name == NULL)
                continue;

            if (param->default_type != NULL) {
                (void)semantic_stage_resolve_type_quiet(
                    param->default_type,
                    ctx,
                    owner,
                    consumer_name,
                    "default-type lookup");
            }

            if (param->constraint != NULL) {
                (void)semantic_stage_resolve_type_quiet(
                    param->constraint,
                    ctx,
                    owner,
                    consumer_name,
                    "generic constraint lookup");
            }
            free(consumer_name);
        }
    }

    if (wc != NULL) {
        for (size_t i = 0; i < wc->count; i++) {
            TypeConstraint *tc = wc->constraints[i];
            char *consumer_name;

            if (tc == NULL)
                continue;

            consumer_name = tc_stage_signature_strdup_fmt(
                "%s %s.%s",
                kind_name != NULL ? kind_name : "decl",
                owner_name != NULL ? owner_name : "<anon>",
                tc->type_param != NULL ? tc->type_param : "<type-param>");
            if (consumer_name == NULL)
                continue;

            for (size_t b = 0; b < tc->bound_count; b++) {
                (void)semantic_stage_resolve_type_quiet(
                    tc->bounds[b],
                    ctx,
                    owner,
                    consumer_name,
                    "where-bound lookup");
            }
            free(consumer_name);
        }
    }

    validate_generic_param_defaults(gp, ctx, owner, kind_name);
    validate_where_clause_bounds(wc, ctx, owner);
}

void
semantic_stage_function_signature(ASTNode *func_decl,
                                  SemanticContext *ctx,
                                  const char *fallback_name)
{
    const char *consumer_name;

    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL || ctx == NULL)
        return;

    consumer_name = func_decl->data.func_decl.name != NULL
        ? func_decl->data.func_decl.name
        : (fallback_name != NULL ? fallback_name : "<func>");

    semantic_stage_generic_contract_nodes(
        func_decl->data.func_decl.generic_params,
        func_decl->data.func_decl.where_clause,
        ctx,
        func_decl,
        "func",
        consumer_name);

    for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
        FuncParam *param = func_decl->data.func_decl.params[i];
        char *param_consumer_name;

        if (param == NULL)
            continue;

        param_consumer_name = tc_stage_signature_strdup_fmt(
            "func %s.%s",
            consumer_name,
            param->name != NULL ? param->name : "<param>");
        if (param_consumer_name == NULL)
            continue;

        (void)semantic_stage_resolve_type_quiet(
            param->type,
            ctx,
            func_decl,
            param_consumer_name,
            "function parameter type lookup");
        free(param_consumer_name);
    }

    (void)semantic_stage_resolve_type_quiet(
        func_decl->data.func_decl.return_type,
        ctx,
        func_decl,
        consumer_name,
        "function return type lookup");

    semantic_stage_required_abilities(
        func_decl->data.func_decl.required_abilities,
        func_decl->data.func_decl.required_ability_count,
        ctx,
        func_decl,
        consumer_name,
        "action ability consumer lookup");
    (void)semantic_stage_named_decl_quiet(
        ctx,
        AST_ZONE_DECL,
        func_decl->data.func_decl.within_zone);
    (void)semantic_stage_named_decl_quiet(
        ctx,
        AST_EFFECT_DECL,
        func_decl->data.func_decl.causes_effect);
}

void
semantic_stage_method_array(ASTNode **methods,
                            size_t method_count,
                            SemanticContext *ctx,
                            const char *fallback_name)
{
    if (methods == NULL || ctx == NULL)
        return;

    for (size_t i = 0; i < method_count; i++)
        semantic_stage_function_signature(methods[i], ctx, fallback_name);
}

void
semantic_stage_event_signature(ASTNode *event_decl,
                               SemanticContext *ctx)
{
    if (event_decl == NULL || event_decl->type != AST_EVENT_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < event_decl->data.event_decl.param_count; i++) {
        ASTNode *param = event_decl->data.event_decl.params[i];
        char *consumer_name;

        if (param == NULL || param->type != AST_LET_DECL)
            continue;

        consumer_name = tc_stage_signature_strdup_fmt(
            "event %s.%s",
            event_decl->data.event_decl.name != NULL
                ? event_decl->data.event_decl.name : "<event>",
            param->data.let_decl.name != NULL
                ? param->data.let_decl.name : "<param>");
        if (consumer_name == NULL)
            continue;

        (void)semantic_stage_resolve_type_quiet(
            param->data.let_decl.type,
            ctx,
            event_decl,
            consumer_name,
            "event parameter type lookup");
        free(consumer_name);
    }

    (void)semantic_stage_resolve_type_quiet(
        event_decl->data.event_decl.return_type,
        ctx,
        event_decl,
        event_decl->data.event_decl.name,
        "event return type lookup");
}
