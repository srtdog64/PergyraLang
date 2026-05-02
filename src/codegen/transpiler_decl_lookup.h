/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend declaration lookup helpers.
 */

#ifndef PERGYRA_TRANSPILER_DECL_LOOKUP_H
#define PERGYRA_TRANSPILER_DECL_LOOKUP_H

#include "transpiler.h"

typedef struct
{
    const MIRDeclMethod *metadata;
    ASTNode           **fallback_methods;
    size_t             count;
    bool               uses_mir_metadata;
    bool               requires_mir_metadata;
} TranspilerHostedMethodView;

static inline TranspilerHostedMethodView
transpiler_hosted_method_view(const TranspilerCtx *ctx,
                              const char *host_name,
                              ASTNode **fallback_methods,
                              size_t fallback_count)
{
    TranspilerHostedMethodView view;
    const MIRDeclHeader *header = NULL;

    view.metadata = NULL;
    view.fallback_methods = fallback_methods;
    view.count = fallback_count;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata = ctx != NULL && ctx->mir != NULL
        && fallback_count > 0;

    if (ctx != NULL && ctx->mir != NULL && host_name != NULL)
        header = mir_find_decl_header(ctx->mir, host_name);
    if (header != NULL) {
        view.metadata = header->method_metadata;
        view.count = header->method_metadata_count;
        view.uses_mir_metadata = true;
    }

    return view;
}

static inline bool
transpiler_hosted_method_view_missing_mir_metadata(
    const TranspilerHostedMethodView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && !view->uses_mir_metadata
        && view->count > 0;
}

static inline TranspilerHostedMethodView
transpiler_hosted_method_view_from_decl(const TranspilerCtx *ctx,
                                        const char *host_name,
                                        ASTNode *decl)
{
    ASTNode **fallback_methods = NULL;
    size_t fallback_count = 0;

    if (decl != NULL) {
        switch (decl->type) {
        case AST_CLASS_DECL:
            fallback_methods = decl->data.class_decl.methods;
            fallback_count = decl->data.class_decl.method_count;
            break;
        case AST_ENUM_DECL:
            fallback_methods = decl->data.enum_decl.methods;
            fallback_count = decl->data.enum_decl.method_count;
            break;
        case AST_PARTY_DECL:
            fallback_methods = decl->data.party_decl.methods;
            fallback_count = decl->data.party_decl.method_count;
            break;
        case AST_ROSTER_DECL:
            fallback_methods = decl->data.roster_decl.methods;
            fallback_count = decl->data.roster_decl.method_count;
            break;
        case AST_WORLD_DECL:
            fallback_methods = decl->data.world_decl.methods;
            fallback_count = decl->data.world_decl.method_count;
            break;
        case AST_RELATION_DECL:
            fallback_methods = decl->data.relation_decl.methods;
            fallback_count = decl->data.relation_decl.method_count;
            break;
        case AST_EFFECT_DECL:
            fallback_methods = decl->data.effect_decl.methods;
            fallback_count = decl->data.effect_decl.method_count;
            break;
        case AST_ZONE_DECL:
            fallback_methods = decl->data.zone_decl.methods;
            fallback_count = decl->data.zone_decl.method_count;
            break;
        default:
            break;
        }
    }

    return transpiler_hosted_method_view(ctx, host_name,
        fallback_methods, fallback_count);
}

static inline ASTNode *
transpiler_hosted_method_view_ast(const TranspilerHostedMethodView *view,
                                  size_t index)
{
    if (view == NULL || index >= view->count)
        return NULL;
    if (view->uses_mir_metadata)
        return view->metadata != NULL ? view->metadata[index].ast : NULL;
    return view->fallback_methods != NULL ? view->fallback_methods[index] : NULL;
}

ASTNode *transpiler_find_named_decl_local(TranspilerCtx *ctx,
                                          ASTNodeType decl_type,
                                          const char *name);
ASTNode *find_role_decl(TranspilerCtx *ctx, const char *role_name);
ASTNode *find_function_decl(TranspilerCtx *ctx, const char *function_name);
ASTNode *find_intent_decl(TranspilerCtx *ctx, const char *intent_name);
ASTNode *find_callable_decl(TranspilerCtx *ctx, const char *name);
ASTNode *find_party_decl(TranspilerCtx *ctx, const char *party_name);
ASTNode *find_roster_decl(TranspilerCtx *ctx, const char *roster_name);
ASTNode *find_enum_decl(TranspilerCtx *ctx, const char *enum_name);
ASTNode *find_class_decl(TranspilerCtx *ctx, const char *class_name);
ASTNode *transpiler_find_type_alias_decl(TranspilerCtx *ctx,
                                         const char *alias_name);
ASTNode *resolve_type_alias_target(TranspilerCtx *ctx, ASTNode *type_node);
ASTNode *find_subject_host_decl(TranspilerCtx *ctx, const char *subject_name);
ASTNode *find_zone_decl(TranspilerCtx *ctx, const char *zone_name);
ASTNode *find_world_decl(TranspilerCtx *ctx, const char *world_name);
ASTNode *find_relation_decl(TranspilerCtx *ctx, const char *relation_name);
ASTNode *find_effect_decl(TranspilerCtx *ctx, const char *effect_name);
ASTNode *find_ability_decl(TranspilerCtx *ctx, const char *ability_name);
ASTNode *find_event_decl(TranspilerCtx *ctx, const char *event_name);
bool transpiler_has_known_nominal_type(TranspilerCtx *ctx, const char *name);
const char *transpiler_decl_name_local(ASTNode *decl);
ASTNode *transpiler_find_decl_in_inventory_local(TranspilerCtx *ctx,
                                                 ASTNodeType decl_type,
                                                 const char *name);
ASTNode *transpiler_find_decl_in_active_inventory_only_local(
    TranspilerCtx *ctx, ASTNodeType decl_type, const char *name);
ASTNode *transpiler_find_host_decl_from_owner_local(TranspilerCtx *ctx,
                                                    const char *owner_name,
                                                    ASTNodeType owner_ast_type);
const char *transpiler_role_subject_name_local(TranspilerCtx *ctx,
                                               const char *role_name);
void transpiler_bind_current_host_decl_local(TranspilerCtx *ctx, ASTNode *decl);
ASTNode *transpiler_current_host_decl_local(TranspilerCtx *ctx);
ASTNode *transpiler_find_nominal_host_decl_local(TranspilerCtx *ctx,
                                                 const char *host_type_name);
ASTNode *current_host_method_decl(TranspilerCtx *ctx,
                                  const char *method_name);
ASTNode *find_nominal_host_method_decl(TranspilerCtx *ctx,
                                       const char *host_type_name,
                                       const char *method_name);

#endif /* PERGYRA_TRANSPILER_DECL_LOOKUP_H */
