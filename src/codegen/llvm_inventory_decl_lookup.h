/* LLVM MIR-backed declaration lookup helpers. */

#ifndef PGY_LLVM_INVENTORY_DECL_LOOKUP_H
#define PGY_LLVM_INVENTORY_DECL_LOOKUP_H

ASTNode *llvm_bind_current_host_decl(LLVMGenCtx *ctx, ASTNode *host_decl);
void llvm_restore_current_host_decl(LLVMGenCtx *ctx, ASTNode *saved_decl);
void llvm_active_inventory(const LLVMGenCtx *ctx,
                           ASTNodeType decl_type,
                           ASTNode ***nodes_out,
                           size_t *count_out);
const char *llvm_decl_node_name(ASTNode *node);
ASTNode *llvm_find_decl_in_active_inventory(const LLVMGenCtx *ctx,
                                            ASTNodeType decl_type,
                                            const char *name);
bool llvm_decl_exists_in_context(const LLVMGenCtx *ctx,
                                 ASTNodeType decl_type,
                                 const char *name);
bool llvm_param_is_implicit_self(const FuncParam *param);
bool llvm_is_host_decl_type(ASTNodeType decl_type);
const MIRDeclHeader *llvm_find_decl_header_in_context_of_type(
    const LLVMGenCtx *ctx,
    ASTNodeType decl_type,
    const char *name);
const MIRDeclHeader *llvm_find_host_decl_header_in_context(
    const LLVMGenCtx *ctx,
    const char *name);
const MIRDeclField *llvm_find_decl_field_in_context(const LLVMGenCtx *ctx,
                                                    const char *host_name,
                                                    const char *field_name);
ASTNode *llvm_mir_decl_field_type(const MIRDeclField *field);
const char *llvm_mir_decl_field_type_name(const MIRDeclField *field);
typedef struct
{
    const MIRDeclHeader *decl_header;
    ASTNode            *ast_compat_decl;
    size_t             ast_compat_count;
    size_t             count;
    bool               uses_mir_metadata;
    bool               requires_mir_metadata;
} LLVMHostedFieldView;
LLVMHostedFieldView llvm_hosted_class_field_view_from_decl(
    const LLVMGenCtx *ctx,
    const char *host_name,
    ASTNode *decl);
bool llvm_hosted_field_view_missing_mir_metadata(
    const LLVMHostedFieldView *view);
const MIRDeclField *llvm_hosted_field_view_metadata(
    const LLVMHostedFieldView *view,
    size_t index);
const char *llvm_hosted_field_view_name(
    const LLVMHostedFieldView *view,
    size_t index);
ASTNode *llvm_hosted_field_view_type(
    const LLVMHostedFieldView *view,
    size_t index);
const char *llvm_hosted_field_view_type_name(
    const LLVMHostedFieldView *view,
    size_t index);
bool llvm_hosted_field_view_find_index(
    const LLVMHostedFieldView *view,
    const char *field_name,
    size_t *index_out);
bool llvm_hosted_field_view_is_subject_like(
    const LLVMHostedFieldView *view,
    size_t index);
typedef struct
{
    const MIRDeclHeader *decl_header;
    ASTNode            *ast_compat_decl;
    size_t             ast_compat_count;
    size_t             count;
    bool               uses_mir_metadata;
    bool               requires_mir_metadata;
} LLVMHostedSharedFieldView;

typedef struct
{
    const MIRDeclHeader *decl_header;
    ASTNode           **ast_compat_slots;
    size_t             ast_compat_count;
    size_t             count;
    bool               uses_mir_metadata;
    bool               requires_mir_metadata;
} LLVMHostedZoneLayerSlotView;

typedef struct
{
    const MIRDeclHeader *decl_header;
    ASTNode           **ast_compat_states;
    size_t             ast_compat_count;
    size_t             count;
    bool               uses_mir_metadata;
    bool               requires_mir_metadata;
} LLVMHostedZoneStateView;

typedef struct LLVMHostedZoneRefreshView
{
    const MIRDeclHeader *decl_header;
    ASTNode           **ast_compat_refreshes;
    size_t             ast_compat_count;
    size_t             count;
    bool               uses_mir_metadata;
    bool               requires_mir_metadata;
} LLVMHostedZoneRefreshView;

typedef struct LLVMHostedDomainSlotView
{
    const MIRDeclHeader *decl_header;
    ASTNode           **ast_compat_slots;
    size_t             ast_compat_count;
    size_t             count;
    bool               uses_mir_metadata;
    bool               requires_mir_metadata;
} LLVMHostedDomainSlotView;

typedef struct
{
    const MIRDeclHeader *decl_header;
    ASTNode           **ast_compat_slots;
    size_t             ast_compat_count;
    size_t             count;
    bool               uses_mir_metadata;
    bool               requires_mir_metadata;
} LLVMHostedWorldZoneSlotView;

typedef struct
{
    const MIRDeclHeader *decl_header;
    ASTNode           **ast_compat_slots;
    size_t             ast_compat_count;
    size_t             count;
    bool               uses_mir_metadata;
    bool               requires_mir_metadata;
} LLVMHostedWorldRosterSlotView;

typedef struct
{
    const MIRDeclHeader *decl_header;
    ASTNode            *ast_compat_decl;
    size_t              ast_compat_count;
    size_t              count;
    bool                uses_mir_metadata;
    bool                requires_mir_metadata;
} LLVMHostedRosterSlotView;

typedef struct
{
    const MIRDeclHeader *decl_header;
    ASTNode            *ast_compat_decl;
    size_t              ast_compat_count;
    size_t              count;
    bool                uses_mir_metadata;
    bool                requires_mir_metadata;
} LLVMHostedRoleSlotView;

LLVMHostedSharedFieldView llvm_hosted_shared_field_view_from_decl(
    const LLVMGenCtx *ctx,
    const char *host_name,
    ASTNode *decl);
bool llvm_hosted_shared_field_view_missing_mir_metadata(
    const LLVMHostedSharedFieldView *view);
const MIRDeclField *llvm_hosted_shared_field_view_metadata(
    const LLVMHostedSharedFieldView *view,
    size_t index);
const char *llvm_hosted_shared_field_view_name(
    const LLVMHostedSharedFieldView *view,
    size_t index);
ASTNode *llvm_hosted_shared_field_view_type(
    const LLVMHostedSharedFieldView *view,
    size_t index);
ASTNode *llvm_hosted_shared_field_view_initializer(
    const LLVMHostedSharedFieldView *view,
    size_t index);
LLVMHostedZoneLayerSlotView llvm_hosted_zone_layer_slot_view_from_decl(
    const LLVMGenCtx *ctx,
    const char *host_name,
    ASTNode *decl);
bool llvm_hosted_zone_layer_slot_view_missing_mir_metadata(
    const LLVMHostedZoneLayerSlotView *view);
const MIRDeclField *llvm_hosted_zone_layer_slot_view_metadata(
    const LLVMHostedZoneLayerSlotView *view,
    size_t index);
const char *llvm_hosted_zone_layer_slot_view_name(
    const LLVMHostedZoneLayerSlotView *view,
    size_t index);
const char *llvm_hosted_zone_layer_slot_view_type_name(
    const LLVMHostedZoneLayerSlotView *view,
    size_t index);
bool llvm_hosted_zone_layer_slot_view_is_relation(
    const LLVMHostedZoneLayerSlotView *view,
    size_t index);
bool llvm_hosted_zone_layer_slot_view_is_pool(
    const LLVMHostedZoneLayerSlotView *view,
    size_t index);
int llvm_hosted_zone_layer_slot_view_pool_capacity(
    const LLVMHostedZoneLayerSlotView *view,
    size_t index);
LLVMHostedZoneStateView llvm_hosted_zone_state_view_from_decl(
    const LLVMGenCtx *ctx,
    const char *host_name,
    ASTNode *decl);
bool llvm_hosted_zone_state_view_missing_mir_metadata(
    const LLVMHostedZoneStateView *view);
const MIRDeclZoneState *llvm_hosted_zone_state_view_metadata(
    const LLVMHostedZoneStateView *view,
    size_t index);
const char *llvm_hosted_zone_state_view_name(
    const LLVMHostedZoneStateView *view,
    size_t index);
const char *llvm_hosted_zone_state_view_layer_slot_name(
    const LLVMHostedZoneStateView *view,
    size_t index);
const char *llvm_hosted_zone_state_view_left_or_target_slot_name(
    const LLVMHostedZoneStateView *view,
    size_t index);
const char *llvm_hosted_zone_state_view_right_slot_name(
    const LLVMHostedZoneStateView *view,
    size_t index);
bool llvm_hosted_zone_state_view_is_relation(
    const LLVMHostedZoneStateView *view,
    size_t index);
LLVMHostedZoneRefreshView llvm_hosted_zone_refresh_view_from_decl(
    const LLVMGenCtx *ctx,
    const char *host_name,
    ASTNode *decl);
bool llvm_hosted_zone_refresh_view_missing_mir_metadata(
    const LLVMHostedZoneRefreshView *view);
const MIRDeclZoneRefresh *llvm_hosted_zone_refresh_view_metadata(
    const LLVMHostedZoneRefreshView *view,
    size_t index);
const char *llvm_hosted_zone_refresh_view_object_slot_name(
    const LLVMHostedZoneRefreshView *view,
    size_t index);
const char *llvm_hosted_zone_refresh_view_source_slot_name(
    const LLVMHostedZoneRefreshView *view,
    size_t index);
const char *llvm_hosted_zone_refresh_view_mapped_source_field(
    const LLVMHostedZoneRefreshView *view,
    size_t index,
    const char *target_field_name);
bool llvm_hosted_zone_refresh_view_mentions_source_field(
    const LLVMHostedZoneRefreshView *view,
    size_t index,
    const char *source_field_name);
LLVMHostedDomainSlotView llvm_hosted_domain_slot_view_from_decl(
    const LLVMGenCtx *ctx,
    const char *host_name,
    ASTNode *decl);
bool llvm_hosted_domain_slot_view_missing_mir_metadata(
    const LLVMHostedDomainSlotView *view);
const MIRDeclField *llvm_hosted_domain_slot_view_metadata(
    const LLVMHostedDomainSlotView *view,
    size_t index);
const char *llvm_hosted_domain_slot_view_name(
    const LLVMHostedDomainSlotView *view,
    size_t index);
ASTNode *llvm_hosted_domain_slot_view_type(
    const LLVMHostedDomainSlotView *view,
    size_t index);
const char *llvm_hosted_domain_slot_view_type_name(
    const LLVMHostedDomainSlotView *view,
    size_t index);
bool llvm_hosted_domain_slot_view_is_subject_like(
    const LLVMHostedDomainSlotView *view,
    size_t index);
bool llvm_hosted_domain_slot_view_is_tobject_like(
    const LLVMHostedDomainSlotView *view,
    size_t index);
bool llvm_hosted_domain_slot_view_is_binding_like(
    const LLVMHostedDomainSlotView *view,
    size_t index);
LLVMHostedWorldZoneSlotView llvm_hosted_world_zone_slot_view_from_decl(
    const LLVMGenCtx *ctx,
    const char *host_name,
    ASTNode *decl);
bool llvm_hosted_world_zone_slot_view_missing_mir_metadata(
    const LLVMHostedWorldZoneSlotView *view);
const MIRDeclField *llvm_hosted_world_zone_slot_view_metadata(
    const LLVMHostedWorldZoneSlotView *view,
    size_t index);
const char *llvm_hosted_world_zone_slot_view_name(
    const LLVMHostedWorldZoneSlotView *view,
    size_t index);
const char *llvm_hosted_world_zone_slot_view_type_name(
    const LLVMHostedWorldZoneSlotView *view,
    size_t index);
LLVMHostedWorldRosterSlotView llvm_hosted_world_roster_slot_view_from_decl(
    const LLVMGenCtx *ctx,
    const char *host_name,
    ASTNode *decl);
bool llvm_hosted_world_roster_slot_view_missing_mir_metadata(
    const LLVMHostedWorldRosterSlotView *view);
const MIRDeclField *llvm_hosted_world_roster_slot_view_metadata(
    const LLVMHostedWorldRosterSlotView *view,
    size_t index);
const char *llvm_hosted_world_roster_slot_view_name(
    const LLVMHostedWorldRosterSlotView *view,
    size_t index);
const char *llvm_hosted_world_roster_slot_view_type_name(
    const LLVMHostedWorldRosterSlotView *view,
    size_t index);
LLVMHostedRosterSlotView llvm_hosted_roster_slot_view_from_decl(
    const LLVMGenCtx *ctx,
    const char *host_name,
    ASTNode *decl);
bool llvm_hosted_roster_slot_view_missing_mir_metadata(
    const LLVMHostedRosterSlotView *view);
const MIRDeclField *llvm_hosted_roster_slot_view_metadata(
    const LLVMHostedRosterSlotView *view,
    size_t index);
const char *llvm_hosted_roster_slot_view_name(
    const LLVMHostedRosterSlotView *view,
    size_t index);
const char *llvm_hosted_roster_slot_view_type_name(
    const LLVMHostedRosterSlotView *view,
    size_t index);
LLVMHostedRoleSlotView llvm_hosted_role_slot_view_from_decl(
    const LLVMGenCtx *ctx,
    const char *host_name,
    ASTNode *decl);
bool llvm_hosted_role_slot_view_missing_mir_metadata(
    const LLVMHostedRoleSlotView *view);
const MIRDeclField *llvm_hosted_role_slot_view_metadata(
    const LLVMHostedRoleSlotView *view,
    size_t index);
const char *llvm_hosted_role_slot_view_name(
    const LLVMHostedRoleSlotView *view,
    size_t index);
bool llvm_hosted_role_slot_view_is_dynamic(
    const LLVMHostedRoleSlotView *view,
    size_t index);
size_t llvm_hosted_role_slot_view_required_ability_count(
    const LLVMHostedRoleSlotView *view,
    size_t index);
const MIRAbilityRef *llvm_hosted_role_slot_view_required_ability_ref(
    const LLVMHostedRoleSlotView *view, size_t index, size_t ability_index);
ASTNode *llvm_find_host_decl_in_active_inventory(const LLVMGenCtx *ctx,
                                                 const char *name);
ASTNode *llvm_current_host_decl(const LLVMGenCtx *ctx);
const char *llvm_current_host_decl_name(const LLVMGenCtx *ctx);

#endif /* PGY_LLVM_INVENTORY_DECL_LOOKUP_H */
