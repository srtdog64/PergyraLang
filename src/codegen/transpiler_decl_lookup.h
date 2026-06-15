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
    const MIRDeclHeader *decl_header;
    ASTNode           **ast_compat_methods;
    size_t             ast_compat_count;
    size_t             count;
    bool               uses_mir_metadata;
    bool               requires_mir_metadata;
} TranspilerHostedMethodView;

typedef enum TranspilerMIRDeclMethodRequirement
{
    TRANSPILER_MIR_DECL_METHOD_REQUIRE_RETURN_TYPE_NAME = 1u << 0,
    TRANSPILER_MIR_DECL_METHOD_REQUIRE_PARAM_TYPE_NAMES = 1u << 1,
    TRANSPILER_MIR_DECL_METHOD_REQUIRE_ALL_TYPE_NAMES =
        TRANSPILER_MIR_DECL_METHOD_REQUIRE_RETURN_TYPE_NAME
        | TRANSPILER_MIR_DECL_METHOD_REQUIRE_PARAM_TYPE_NAMES
} TranspilerMIRDeclMethodRequirement;

typedef struct
{
    const MIRDeclHeader *decl_header;
    ClassField        **ast_compat_fields;
    size_t             ast_compat_count;
    size_t             count;
    bool               uses_mir_metadata;
    bool               requires_mir_metadata;
} TranspilerHostedFieldView;

typedef struct
{
    const MIRDeclHeader *decl_header;
    ASTNode           **ast_compat_fields;
    size_t             ast_compat_count;
    size_t             count;
    bool               uses_mir_metadata;
    bool               requires_mir_metadata;
} TranspilerHostedSharedFieldView;

typedef struct
{
    const MIRDeclHeader *decl_header;
    ASTNode           **ast_compat_slots;
    size_t             ast_compat_count;
    size_t             count;
    bool               uses_mir_metadata;
    bool               requires_mir_metadata;
} TranspilerHostedZoneLayerSlotView;

typedef struct
{
    const MIRDeclHeader *decl_header;
    ASTNode           **ast_compat_slots;
    size_t             ast_compat_count;
    size_t             count;
    bool               uses_mir_metadata;
    bool               requires_mir_metadata;
} TranspilerHostedDomainSlotView;

typedef struct
{
    const MIRDeclHeader *decl_header;
    ASTNode           **ast_compat_slots;
    size_t             ast_compat_count;
    size_t             count;
    bool               uses_mir_metadata;
    bool               requires_mir_metadata;
} TranspilerHostedWorldZoneSlotView;

typedef struct
{
    const MIRDeclHeader *decl_header;
    ASTNode           **ast_compat_slots;
    size_t             ast_compat_count;
    size_t             count;
    bool               uses_mir_metadata;
    bool               requires_mir_metadata;
} TranspilerHostedWorldRosterSlotView;

typedef struct
{
    const MIRDeclHeader *decl_header;
    ASTNode            *ast_compat_decl;
    size_t              ast_compat_count;
    size_t              count;
    bool                uses_mir_metadata;
    bool                requires_mir_metadata;
} TranspilerHostedRosterSlotView;

typedef struct
{
    const MIRDeclHeader *decl_header;
    ASTNode            *ast_compat_decl;
    size_t              ast_compat_count;
    size_t              count;
    bool                uses_mir_metadata;
    bool                requires_mir_metadata;
} TranspilerHostedRoleSlotView;

TranspilerHostedMethodView transpiler_hosted_method_view(
    const TranspilerCtx *ctx,
    const char *host_name,
    ASTNode **ast_compat_methods,
    size_t ast_compat_count);
bool transpiler_hosted_method_view_missing_mir_metadata(
    const TranspilerHostedMethodView *view);
bool transpiler_hosted_method_view_missing_mir_method_row(
    const TranspilerHostedMethodView *view,
    size_t index);
bool transpiler_require_hosted_method_view_rows(
    TranspilerCtx *ctx,
    const TranspilerHostedMethodView *view,
    const char *message_fmt,
    const char *host_name);
const MIRDeclMethod *transpiler_hosted_method_view_metadata(
    const TranspilerHostedMethodView *view,
    size_t index);
const MIRDeclMethod *transpiler_find_host_method_metadata_in_context(
    const TranspilerCtx *ctx,
    const char *host_type_name,
    const char *method_name);
bool transpiler_mir_decl_method_metadata_complete_for(
    TranspilerCtx *ctx,
    const MIRDeclMethod *method,
    const char *host_name,
    const char *method_name,
    unsigned requirements,
    const char *missing_return_type_fmt,
    const char *missing_param_type_fmt);
const char *transpiler_mir_decl_method_name(const MIRDeclMethod *method);
size_t transpiler_mir_decl_method_param_count(const MIRDeclMethod *method);
FuncParam *transpiler_mir_decl_method_param(const MIRDeclMethod *method,
                                            size_t index);
const char *transpiler_mir_decl_method_param_type_name(
    const MIRDeclMethod *method,
    size_t index);
ASTNode *transpiler_mir_decl_method_return_type(const MIRDeclMethod *method);
const char *transpiler_mir_decl_method_return_type_name(
    const MIRDeclMethod *method);
bool transpiler_mir_decl_method_is_async(const MIRDeclMethod *method);
bool transpiler_mir_decl_method_is_action_like(const MIRDeclMethod *method);
const char *transpiler_mir_decl_method_within_zone(
    const MIRDeclMethod *method);
const char *transpiler_mir_decl_method_causes_effect(
    const MIRDeclMethod *method);
const MIRRoutine *transpiler_mir_decl_method_routine(
    const TranspilerCtx *ctx,
    const MIRDeclMethod *method);
size_t transpiler_mir_decl_method_projection_write_count(
    const MIRDeclMethod *method);
const char *transpiler_mir_decl_method_projection_write_root_name(
    const MIRDeclMethod *method,
    size_t index);
const char *transpiler_mir_decl_method_projection_write_member_name(
    const MIRDeclMethod *method,
    size_t index);
size_t transpiler_mir_decl_method_projection_call_count(
    const MIRDeclMethod *method);
const char *transpiler_mir_decl_method_projection_call_receiver_name(
    const MIRDeclMethod *method,
    size_t index);
const char *transpiler_mir_decl_method_projection_call_method_name(
    const MIRDeclMethod *method,
    size_t index);
TranspilerHostedMethodView transpiler_hosted_method_view_from_decl(
    const TranspilerCtx *ctx,
    const char *host_name,
    ASTNode *decl);
TranspilerHostedFieldView transpiler_hosted_class_field_view_from_decl(
    const TranspilerCtx *ctx,
    const char *host_name,
    ASTNode *decl);
bool transpiler_hosted_field_view_missing_mir_metadata(
    const TranspilerHostedFieldView *view);
const MIRDeclField *transpiler_hosted_field_view_metadata(
    const TranspilerHostedFieldView *view,
    size_t index);
const char *transpiler_hosted_field_view_name(
    const TranspilerHostedFieldView *view,
    size_t index);
ASTNode *transpiler_hosted_field_view_type(
    const TranspilerHostedFieldView *view,
    size_t index);
bool transpiler_hosted_field_view_find_index(
    const TranspilerHostedFieldView *view,
    const char *field_name,
    size_t *index_out);
bool transpiler_hosted_field_view_is_subject_like(
    const TranspilerHostedFieldView *view,
    size_t index);
TranspilerHostedSharedFieldView transpiler_hosted_shared_field_view_from_decl(
    const TranspilerCtx *ctx,
    const char *host_name,
    ASTNode *decl);
bool transpiler_hosted_shared_field_view_missing_mir_metadata(
    const TranspilerHostedSharedFieldView *view);
const MIRDeclField *transpiler_hosted_shared_field_view_metadata(
    const TranspilerHostedSharedFieldView *view,
    size_t index);
const char *transpiler_hosted_shared_field_view_name(
    const TranspilerHostedSharedFieldView *view,
    size_t index);
ASTNode *transpiler_hosted_shared_field_view_type(
    const TranspilerHostedSharedFieldView *view,
    size_t index);
ASTNode *transpiler_hosted_shared_field_view_initializer(
    const TranspilerHostedSharedFieldView *view,
    size_t index);
TranspilerHostedZoneLayerSlotView
transpiler_hosted_zone_layer_slot_view_from_decl(const TranspilerCtx *ctx,
                                                 const char *host_name,
                                                 ASTNode *decl);
bool transpiler_hosted_zone_layer_slot_view_missing_mir_metadata(
    const TranspilerHostedZoneLayerSlotView *view);
const MIRDeclField *transpiler_hosted_zone_layer_slot_view_metadata(
    const TranspilerHostedZoneLayerSlotView *view,
    size_t index);
const char *transpiler_hosted_zone_layer_slot_view_name(
    const TranspilerHostedZoneLayerSlotView *view,
    size_t index);
ASTNode *transpiler_hosted_zone_layer_slot_view_type(
    const TranspilerHostedZoneLayerSlotView *view,
    size_t index);
const char *transpiler_hosted_zone_layer_slot_view_type_name(
    const TranspilerHostedZoneLayerSlotView *view,
    size_t index);
bool transpiler_hosted_zone_layer_slot_view_is_relation(
    const TranspilerHostedZoneLayerSlotView *view,
    size_t index);
bool transpiler_hosted_zone_layer_slot_view_is_pool(
    const TranspilerHostedZoneLayerSlotView *view,
    size_t index);
int transpiler_hosted_zone_layer_slot_view_pool_capacity(
    const TranspilerHostedZoneLayerSlotView *view,
    size_t index);
TranspilerHostedDomainSlotView
transpiler_hosted_domain_slot_view_from_decl(const TranspilerCtx *ctx,
                                             const char *host_name,
                                             ASTNode *decl);
bool transpiler_hosted_domain_slot_view_missing_mir_metadata(
    const TranspilerHostedDomainSlotView *view);
const MIRDeclField *transpiler_hosted_domain_slot_view_metadata(
    const TranspilerHostedDomainSlotView *view,
    size_t index);
const char *transpiler_hosted_domain_slot_view_name(
    const TranspilerHostedDomainSlotView *view,
    size_t index);
ASTNode *transpiler_hosted_domain_slot_view_type(
    const TranspilerHostedDomainSlotView *view,
    size_t index);
const char *transpiler_hosted_domain_slot_view_type_name(
    const TranspilerHostedDomainSlotView *view,
    size_t index);
bool transpiler_hosted_domain_slot_view_is_subject_like(
    const TranspilerHostedDomainSlotView *view,
    size_t index);
bool transpiler_hosted_domain_slot_view_is_tobject_like(
    const TranspilerHostedDomainSlotView *view,
    size_t index);
bool transpiler_hosted_domain_slot_view_is_binding_like(
    const TranspilerHostedDomainSlotView *view,
    size_t index);
TranspilerHostedWorldZoneSlotView
transpiler_hosted_world_zone_slot_view_from_decl(const TranspilerCtx *ctx,
                                                 const char *host_name,
                                                 ASTNode *decl);
bool transpiler_hosted_world_zone_slot_view_missing_mir_metadata(
    const TranspilerHostedWorldZoneSlotView *view);
const MIRDeclField *transpiler_hosted_world_zone_slot_view_metadata(
    const TranspilerHostedWorldZoneSlotView *view,
    size_t index);
const char *transpiler_hosted_world_zone_slot_view_name(
    const TranspilerHostedWorldZoneSlotView *view,
    size_t index);
const char *transpiler_hosted_world_zone_slot_view_type_name(
    const TranspilerHostedWorldZoneSlotView *view,
    size_t index);
TranspilerHostedWorldRosterSlotView
transpiler_hosted_world_roster_slot_view_from_decl(const TranspilerCtx *ctx,
                                                   const char *host_name,
                                                   ASTNode *decl);
bool transpiler_hosted_world_roster_slot_view_missing_mir_metadata(
    const TranspilerHostedWorldRosterSlotView *view);
const MIRDeclField *transpiler_hosted_world_roster_slot_view_metadata(
    const TranspilerHostedWorldRosterSlotView *view,
    size_t index);
const char *transpiler_hosted_world_roster_slot_view_name(
    const TranspilerHostedWorldRosterSlotView *view,
    size_t index);
const char *transpiler_hosted_world_roster_slot_view_type_name(
    const TranspilerHostedWorldRosterSlotView *view,
    size_t index);
TranspilerHostedRosterSlotView transpiler_hosted_roster_slot_view_from_decl(
    const TranspilerCtx *ctx,
    const char *host_name,
    ASTNode *decl);
bool transpiler_hosted_roster_slot_view_missing_mir_metadata(
    const TranspilerHostedRosterSlotView *view);
const MIRDeclField *transpiler_hosted_roster_slot_view_metadata(
    const TranspilerHostedRosterSlotView *view,
    size_t index);
const char *transpiler_hosted_roster_slot_view_name(
    const TranspilerHostedRosterSlotView *view,
    size_t index);
const char *transpiler_hosted_roster_slot_view_type_name(
    const TranspilerHostedRosterSlotView *view,
    size_t index);
TranspilerHostedRoleSlotView transpiler_hosted_role_slot_view_from_decl(
    const TranspilerCtx *ctx,
    const char *host_name,
    ASTNode *decl);
bool transpiler_hosted_role_slot_view_missing_mir_metadata(
    const TranspilerHostedRoleSlotView *view);
const MIRDeclField *transpiler_hosted_role_slot_view_metadata(
    const TranspilerHostedRoleSlotView *view,
    size_t index);
const char *transpiler_hosted_role_slot_view_name(
    const TranspilerHostedRoleSlotView *view,
    size_t index);
bool transpiler_hosted_role_slot_view_is_dynamic(
    const TranspilerHostedRoleSlotView *view,
    size_t index);
size_t transpiler_hosted_role_slot_view_required_ability_count(
    const TranspilerHostedRoleSlotView *view,
    size_t index);
const MIRAbilityRef *transpiler_hosted_role_slot_view_required_ability_ref(
    const TranspilerHostedRoleSlotView *view,
    size_t index,
    size_t ability_index);
const MIRDeclField *transpiler_find_decl_field_metadata(
    const TranspilerCtx *ctx,
    const char *host_name,
    const char *field_name);
ASTNode *transpiler_mir_decl_field_type(const MIRDeclField *field);
const char *transpiler_mir_decl_field_type_name(const MIRDeclField *field);
MIRDeclFieldKind transpiler_mir_decl_field_kind_or(
    const MIRDeclField *field,
    MIRDeclFieldKind fallback);
bool transpiler_mir_decl_field_is_subject_like(const MIRDeclField *field);
bool transpiler_mir_decl_field_is_tobject_like(const MIRDeclField *field);

ASTNode *transpiler_find_named_decl_local(TranspilerCtx *ctx,
                                          ASTNodeType decl_type,
                                          const char *name);
ASTNode *find_role_decl(TranspilerCtx *ctx, const char *role_name);
ASTNode *find_function_decl(TranspilerCtx *ctx, const char *function_name);
bool transpiler_decl_is_extern_function(const TranspilerCtx *ctx,
                                        const ASTNode *decl);
ASTNode *find_intent_decl(TranspilerCtx *ctx, const char *intent_name);
ASTNode *find_callable_decl(TranspilerCtx *ctx, const char *name);
ASTNode *find_party_decl(TranspilerCtx *ctx, const char *party_name);
ASTNode *find_roster_decl(TranspilerCtx *ctx, const char *roster_name);
ASTNode *find_enum_decl(TranspilerCtx *ctx, const char *enum_name);
ASTNode *find_class_decl(TranspilerCtx *ctx, const char *class_name);
ASTNode *transpiler_find_projection_nominal_decl_local(TranspilerCtx *ctx,
                                                       const char *name);
ASTNode *transpiler_find_type_alias_decl(TranspilerCtx *ctx,
                                         const char *alias_name);
ASTNode *resolve_type_alias_target(TranspilerCtx *ctx, ASTNode *type_node);
ASTNode *find_subject_host_decl(TranspilerCtx *ctx, const char *subject_name);
ASTNode *find_ability_decl(TranspilerCtx *ctx, const char *ability_name);
ASTNode *find_event_decl(TranspilerCtx *ctx, const char *event_name);
bool transpiler_has_known_nominal_type(TranspilerCtx *ctx, const char *name);
ASTNode *transpiler_find_domain_constructor_decl_local(TranspilerCtx *ctx,
                                                       const char *name);
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
