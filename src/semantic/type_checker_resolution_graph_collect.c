#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "type_checker_internal.h"

static char *
resolution_collect_strdup_fmt(const char *fmt, ...)
{
    va_list ap;
    va_list ap2;
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

void
semantic_type_resolution_collect_type_refs(ASTNode *type_node,
                                           SemanticContext *ctx,
                                           const ASTNode *consumer_site,
                                           const char *consumer_name,
                                           const char *reason)
{
    if (type_node == NULL || ctx == NULL || consumer_name == NULL)
        return;

    switch (type_node->type) {
    case AST_CHANNEL_TYPE:
        semantic_type_resolution_collect_type_refs(
            type_node->data.channel_type.element_type,
            ctx,
            consumer_site,
            consumer_name,
            reason);
        semantic_type_resolution_try_record_stable_constructed_type(ctx, type_node);
        return;

    case AST_FUTURE_TYPE:
        semantic_type_resolution_collect_type_refs(
            type_node->data.future_type.value_type,
            ctx,
            consumer_site,
            consumer_name,
            reason);
        semantic_type_resolution_try_record_stable_constructed_type(ctx, type_node);
        return;

    case AST_EVENT_HANDLER_TYPE:
        for (size_t i = 0; i < type_node->data.event_handler_type.param_count; i++) {
            semantic_type_resolution_collect_type_refs(
                type_node->data.event_handler_type.param_types[i],
                ctx,
                consumer_site,
                consumer_name,
                reason);
        }
        semantic_type_resolution_collect_type_refs(
            type_node->data.event_handler_type.return_type,
            ctx,
            consumer_site,
            consumer_name,
            reason);
        semantic_type_resolution_try_record_stable_constructed_type(ctx, type_node);
        return;

    case AST_TYPE:
        if (type_node->data.type.tuple_elements != NULL) {
            for (size_t i = 0; i < type_node->data.type.tuple_element_count; i++) {
                semantic_type_resolution_collect_type_refs(
                    type_node->data.type.tuple_elements[i],
                    ctx,
                    consumer_site,
                    consumer_name,
                    reason);
            }
            semantic_type_resolution_try_record_stable_constructed_type(ctx, type_node);
            return;
        }
        if (type_node->data.type.name != NULL) {
            semantic_type_resolution_record_type_ref_dependency(
                ctx,
                consumer_site != NULL ? consumer_site : type_node,
                consumer_name,
                type_node,
                reason != NULL ? reason : "type dependency");
        }
        if (type_node->data.type.generic_args != NULL) {
            for (size_t i = 0; i < type_node->data.type.generic_args->count; i++) {
                GenericParam *gp = type_node->data.type.generic_args->params[i];
                if (gp != NULL && gp->constraint != NULL) {
                    semantic_type_resolution_collect_type_refs(
                        gp->constraint,
                        ctx,
                        consumer_site,
                        consumer_name,
                        reason);
                }
            }
        }
        semantic_type_resolution_try_record_stable_constructed_type(ctx, type_node);
        return;

    default:
        return;
    }
}

void
semantic_type_resolution_collect_generic_contract_inventory(GenericParams *gp,
                                                            WhereClause *wc,
                                                            SemanticContext *ctx,
                                                            const ASTNode *owner,
                                                            const char *owner_kind,
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

            consumer_name = resolution_collect_strdup_fmt(
                "%s %s.%s",
                owner_kind != NULL ? owner_kind : "decl",
                owner_name != NULL ? owner_name : "<anon>",
                param->name != NULL ? param->name : "<type-param>");
            if (consumer_name == NULL)
                continue;

            semantic_type_resolution_collect_type_refs(
                param->default_type,
                ctx,
                owner,
                consumer_name,
                "default-type lookup");
            semantic_type_resolution_collect_type_refs(
                param->constraint,
                ctx,
                owner,
                consumer_name,
                "generic constraint lookup");
            free(consumer_name);
        }
    }

    if (wc != NULL) {
        for (size_t i = 0; i < wc->count; i++) {
            TypeConstraint *tc = wc->constraints[i];
            char *consumer_name;

            if (tc == NULL)
                continue;

            consumer_name = resolution_collect_strdup_fmt(
                "%s %s.%s",
                owner_kind != NULL ? owner_kind : "decl",
                owner_name != NULL ? owner_name : "<anon>",
                tc->type_param != NULL ? tc->type_param : "<type-param>");
            if (consumer_name == NULL)
                continue;

            for (size_t b = 0; b < tc->bound_count; b++) {
                semantic_type_resolution_collect_type_refs(
                    tc->bounds[b],
                    ctx,
                    owner != NULL ? owner : tc->bounds[b],
                    consumer_name,
                    "where-bound lookup");
            }
            free(consumer_name);
        }
    }
}

void
semantic_type_resolution_record_string_dependency(SemanticContext *ctx,
                                                  const ASTNode *consumer_site,
                                                  const char *consumer_name,
                                                  const char *provider_name,
                                                  const char *reason)
{
    if (provider_name == NULL || provider_name[0] == '\0')
        return;

    semantic_type_resolution_record_named_dependency(
        ctx,
        consumer_site,
        consumer_name,
        TYPE_RES_NODE_DECL,
        NULL,
        provider_name,
        reason);
}

void
semantic_type_resolution_precollect_required_abilities(ASTNode **ability_refs,
                                                       size_t ability_count,
                                                       SemanticContext *ctx,
                                                       const ASTNode *owner,
                                                       const char *consumer_name,
                                                       const char *reason)
{
    if (ability_refs == NULL || ctx == NULL)
        return;

    for (size_t i = 0; i < ability_count; i++) {
        semantic_type_resolution_collect_type_refs(
            ability_refs[i],
            ctx,
            owner,
            consumer_name,
            reason);
    }
}

void
semantic_type_resolution_register_top_level_decl(ASTNode *stmt,
                                                 SemanticContext *ctx)
{
    const char *label = NULL;
    TypeResolutionNodeKind kind = TYPE_RES_NODE_DECL;

    if (stmt == NULL || ctx == NULL)
        return;

    switch (stmt->type) {
    case AST_TYPE_ALIAS:
        label = stmt->data.type_alias.name;
        kind = TYPE_RES_NODE_ALIAS;
        break;
    case AST_CLASS_DECL:
        label = stmt->data.class_decl.name;
        break;
    case AST_FUNC_DECL:
        label = stmt->data.func_decl.name;
        break;
    case AST_EVENT_DECL:
        label = stmt->data.event_decl.name;
        break;
    case AST_ENUM_DECL:
        label = stmt->data.enum_decl.name;
        break;
    case AST_ABILITY_DECL:
        label = stmt->data.ability_decl.name;
        break;
    case AST_ROLE_DECL:
        label = stmt->data.role_decl.name;
        break;
    case AST_PARTY_DECL:
        label = ast_party_name(stmt);
        break;
    case AST_ROSTER_DECL:
        label = ast_roster_name(stmt);
        break;
    case AST_WORLD_DECL:
        label = ast_world_name(stmt);
        break;
    case AST_INTENT_DECL:
        label = stmt->data.intent_decl.name;
        break;
    case AST_RELATION_DECL:
        label = ast_relation_name(stmt);
        break;
    case AST_EFFECT_DECL:
        label = ast_effect_name(stmt);
        break;
    case AST_ZONE_DECL:
        label = ast_zone_name(stmt);
        break;
    default:
        return;
    }

    if (label == NULL || label[0] == '\0')
        return;

    (void)type_resolution_intern_node(&ctx->type_resolution_graph,
                                      kind,
                                      stmt,
                                      label);
}
