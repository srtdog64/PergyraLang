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
    ASTNode           **ast_compat_methods;
    size_t             ast_compat_count;
    size_t             count;
    bool               uses_mir_metadata;
    bool               requires_mir_metadata;
} TranspilerHostedMethodView;

TranspilerHostedMethodView transpiler_hosted_method_view(
    const TranspilerCtx *ctx,
    const char *host_name,
    ASTNode **ast_compat_methods,
    size_t ast_compat_count);
bool transpiler_hosted_method_view_missing_mir_metadata(
    const TranspilerHostedMethodView *view);
const MIRDeclMethod *transpiler_hosted_method_view_metadata(
    const TranspilerHostedMethodView *view,
    size_t index);
const char *transpiler_mir_decl_method_name(const MIRDeclMethod *method);
ASTNode *transpiler_mir_decl_method_source_ast(const MIRDeclMethod *method);
size_t transpiler_mir_decl_method_param_count(const MIRDeclMethod *method);
FuncParam *transpiler_mir_decl_method_param(const MIRDeclMethod *method,
                                            size_t index);
ASTNode *transpiler_mir_decl_method_return_type(const MIRDeclMethod *method);
bool transpiler_mir_decl_method_is_action_like(const MIRDeclMethod *method);
const MIRRoutine *transpiler_mir_decl_method_routine(
    const TranspilerCtx *ctx,
    const MIRDeclMethod *method);
const MIRRoutine *transpiler_hosted_method_view_routine(
    const TranspilerCtx *ctx,
    const TranspilerHostedMethodView *view,
    size_t index);
TranspilerHostedMethodView transpiler_hosted_method_view_from_decl(
    const TranspilerCtx *ctx,
    const char *host_name,
    ASTNode *decl);
ASTNode *transpiler_hosted_method_view_source_ast(
    const TranspilerHostedMethodView *view,
    size_t index);

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
bool transpiler_is_host_decl_type(ASTNodeType decl_type);
ASTNode *transpiler_find_decl_in_inventory_local(TranspilerCtx *ctx,
                                                 ASTNodeType decl_type,
                                                 const char *name);
ASTNode *transpiler_find_decl_in_active_inventory_only_local(
    TranspilerCtx *ctx, ASTNodeType decl_type, const char *name);
ASTNode *transpiler_find_host_decl_from_owner_local(TranspilerCtx *ctx,
                                                    const char *owner_name,
                                                    ASTNodeType owner_ast_type);
ASTNode *transpiler_role_subject_type_node_local(ASTNode *role_decl);
const char *transpiler_role_subject_type_name_local(ASTNode *role_decl);
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
