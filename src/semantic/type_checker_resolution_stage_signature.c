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
            && ast_type_name(ability_ref) != NULL) {
            ctx->type_resolution_dag_ability_evidence_count++;
            (void)semantic_stage_named_decl_quiet(
                ctx,
                AST_ABILITY_DECL,
                ast_type_name(ability_ref));
        }

        (void)semantic_stage_resolve_type_quiet(
            ability_ref,
            ctx,
            owner,
            consumer_name,
            reason);
    }
}

static void
semantic_record_dag_generic_contract_evidence(SemanticContext *ctx)
{
    if (ctx == NULL)
        return;
    ctx->type_resolution_dag_generic_contract_evidence_count++;
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
        size_t generic_count = ast_generic_param_count(gp);
        for (size_t i = 0; i < generic_count; i++) {
            GenericParam *param = ast_generic_param_at(gp, i);
            const char *param_name = ast_generic_param_name(param);
            ASTNode *default_type = ast_generic_param_default_type(param);
            ASTNode *constraint = ast_generic_param_constraint(param);
            char *consumer_name;

            if (param == NULL)
                continue;

            consumer_name = tc_stage_signature_strdup_fmt(
                "%s %s.%s",
                kind_name != NULL ? kind_name : "decl",
                owner_name != NULL ? owner_name : "<anon>",
                param_name != NULL ? param_name : "<type-param>");
            if (consumer_name == NULL)
                continue;

            if (default_type != NULL) {
                semantic_record_dag_generic_contract_evidence(ctx);
                (void)semantic_stage_resolve_type_quiet(
                    default_type,
                    ctx,
                    owner,
                    consumer_name,
                    "default-type lookup");
            }

            if (constraint != NULL) {
                semantic_record_dag_generic_contract_evidence(ctx);
                (void)semantic_stage_resolve_type_quiet(
                    constraint,
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
                semantic_record_dag_generic_contract_evidence(ctx);
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
                                  const char *owner_name_hint)
{
    const char *consumer_name;

    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL || ctx == NULL)
        return;

    consumer_name = ast_declaration_name(func_decl) != NULL
        ? ast_declaration_name(func_decl)
        : (owner_name_hint != NULL ? owner_name_hint : "<func>");

    semantic_stage_generic_contract_nodes(
        ast_func_generic_params(func_decl),
        ast_func_where_clause(func_decl),
        ctx,
        func_decl,
        "func",
        consumer_name);

    for (size_t i = 0; i < ast_func_param_count(func_decl); i++) {
        FuncParam *param = ast_func_param(func_decl, i);
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
        ast_func_return_type(func_decl),
        ctx,
        func_decl,
        consumer_name,
        "function return type lookup");

    semantic_stage_required_abilities(
        ast_func_required_abilities(func_decl, NULL),
        ast_func_required_ability_count(func_decl),
        ctx,
        func_decl,
        consumer_name,
        "action ability consumer lookup");
    (void)semantic_stage_named_decl_quiet(
        ctx,
        AST_ZONE_DECL,
        ast_func_within_zone(func_decl));
    (void)semantic_stage_named_decl_quiet(
        ctx,
        AST_EFFECT_DECL,
        ast_func_causes_effect(func_decl));
}

void
semantic_stage_method_array(ASTNode **methods,
                            size_t method_count,
                            SemanticContext *ctx,
                            const char *owner_name_hint)
{
    if (methods == NULL || ctx == NULL)
        return;

    for (size_t i = 0; i < method_count; i++)
        semantic_stage_function_signature(methods[i], ctx, owner_name_hint);
}

void
semantic_stage_event_signature(ASTNode *event_decl,
                               SemanticContext *ctx)
{
    if (event_decl == NULL || event_decl->type != AST_EVENT_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < ast_event_param_count(event_decl); i++) {
        ASTNode *param = ast_event_param(event_decl, i);
        char *consumer_name;

        if (param == NULL || param->type != AST_LET_DECL)
            continue;

        consumer_name = tc_stage_signature_strdup_fmt(
            "event %s.%s",
            ast_event_name(event_decl) != NULL
                ? ast_event_name(event_decl) : "<event>",
            ast_let_name(param) != NULL
                ? ast_let_name(param) : "<param>");
        if (consumer_name == NULL)
            continue;

        (void)semantic_stage_resolve_type_quiet(
            ast_let_type(param),
            ctx,
            event_decl,
            consumer_name,
            "event parameter type lookup");
        free(consumer_name);
    }

    (void)semantic_stage_resolve_type_quiet(
        ast_event_return_type(event_decl),
        ctx,
        event_decl,
        ast_event_name(event_decl),
        "event return type lookup");
}
