/*
 * Domain-heavy AST payload shapes.
 *
 * These are named data structs rather than include fragments, so ast.h can
 * expose the ASTNode union shape without owning every domain field layout.
 */

#ifndef PERGYRA_AST_DOMAIN_DATA_H
#define PERGYRA_AST_DOMAIN_DATA_H

#include "ast_types.h"

typedef struct {
    char* name;
    ASTNode** role_slots;
    size_t role_count;
    ASTNode** shared_fields;
    size_t shared_count;
    ASTNode** methods;
    size_t method_count;
    ASTNode* extends;
    GenericParams* generic_params;
    StructuredComment* doc_comment;
} ASTPartyDeclData;

typedef struct {
    char* slot_name;
    ASTNode** required_abilities;
    size_t ability_count;
    bool is_array;
    bool is_dynamic;
} ASTRoleSlotData;

typedef struct {
    char* name;
    ASTNode* type;
    ASTNode* initializer;
    AccessModifier access;
} ASTPartySharedData;

typedef struct {
    char* method_name;
    char* role_slot_name;
    ASTNode* ability_type;
} ASTContextAccessData;

typedef struct {
    char* party_type;
    struct {
        char* slot_name;
        ASTNode* value;
    }* assignments;
    size_t assignment_count;
} ASTPartyInstanceData;

typedef struct {
    char* name;
    ASTNode** party_slots;
    size_t party_count;
    ASTNode** shared_fields;
    size_t shared_count;
    ASTNode** methods;
    size_t method_count;
    GenericParams* generic_params;
    StructuredComment* doc_comment;
} ASTRosterDeclData;

typedef struct {
    char* slot_name;
    char* party_type;
    bool is_array;
} ASTRosterSlotData;

typedef struct {
    char* name;
    ASTNode** rosters;
    size_t roster_count;
    ASTNode** zones;
    size_t zone_count;
    ASTNode** shared_fields;
    size_t shared_count;
    ASTNode** methods;
    size_t method_count;
    ASTNode** activations;
    size_t activate_count;
    ASTNode** deactivations;
    size_t deactivate_count;
    ASTNode** maintained_zones;
    size_t maintained_zone_count;
    ASTNode** states;
    size_t state_count;
    StructuredComment* doc_comment;
} ASTWorldDeclData;

typedef struct {
    char* slot_name;
    char* roster_type;
    ASTNode* initializer;
} ASTWorldRosterData;

typedef struct {
    char* slot_name;
    char* zone_type;
    ASTNode* initializer;
} ASTWorldZoneData;

typedef struct {
    char* zone_slot_name;
    char* state_name;
} ASTWorldActivateData;

typedef ASTWorldActivateData ASTWorldDeactivateData;
typedef ASTWorldActivateData ASTWorldMaintainData;

typedef struct {
    char* state_name;
    char* zone_slot_name;
    WorldStateSourceKind source_kind;
    char* detail_name;
    char** input_names;
    size_t input_count;
} ASTWorldStateData;

typedef struct {
    char* name;
    ASTNode** involves;
    size_t involve_count;
    ASTNode** values;
    size_t value_count;
    ASTNode** bindings;
    size_t binding_count;
    ASTNode** steps;
    size_t step_count;
    bool is_concurrent;
    IntentRollbackPolicy rollback_policy;
    ASTNode* priority_expr;
    ASTNode* success_expr;
    ASTNode* failure_expr;
    StructuredComment* doc_comment;
    char** default_who_names;
    size_t default_who_count;
    ASTNode* default_where_type;
} ASTIntentDeclData;

typedef struct {
    char* alias;
    ASTNode* subject_type;
} ASTIntentInvolvesData;

typedef struct {
    char* alias;
    ASTNode* value_type;
} ASTIntentValueData;

typedef struct {
    char* name;
    ASTNode* where_type;
    ASTNode* using_expr;
    ASTNode* intent_expr;
    char* transfer_from_alias;
    char* transfer_to_alias;
    char** who_names;
    size_t who_count;
    ASTNode** on_exprs;
    size_t on_expr_count;
    ASTNode** compensate_exprs;
    size_t compensate_expr_count;
    ASTNode* pre_expr;
    ASTNode* guard_expr;
    ASTNode* post_expr;
    ASTNode* invariant_expr;
    ASTNode** required_abilities;
    size_t required_ability_count;
    char* causes_effect;
    char** authorized_by;
    size_t authorized_by_count;
    ASTNode* expect_expr;
    bool inherited_who_from_action;
    bool inherited_where_from_action;
    bool inherited_requires_from_action;
    bool inherited_causes_from_action;
    bool inherited_authorized_by_from_action;
    bool derived_where_from_using;
    bool derived_where_from_transfer;
    bool derived_using_from_transfer;
} ASTIntentStepData;

typedef struct {
    char* name;
    ASTNode** slots;
    size_t slot_count;
    ASTNode** refreshes;
    size_t refresh_count;
    ASTNode** shared_fields;
    size_t shared_count;
    ASTNode** methods;
    size_t method_count;
    StructuredComment* doc_comment;
    RelationEndpointKind between_left_kind;
    RelationEndpointKind between_right_kind;
    ASTNode* between_left_type;
    ASTNode* between_right_type;
    bool between_left_many;
    bool between_right_many;
} ASTRelationDeclData;

typedef struct {
    char* name;
    ASTNode** slots;
    size_t slot_count;
    ASTNode** refreshes;
    size_t refresh_count;
    ASTNode** shared_fields;
    size_t shared_count;
    ASTNode** methods;
    size_t method_count;
    StructuredComment* doc_comment;
} ASTEffectDeclData;

typedef struct {
    char* name;
    ASTNode** slots;
    size_t slot_count;
    ASTNode** layer_slots;
    size_t layer_slot_count;
    ASTNode** applies;
    size_t apply_count;
    ASTNode** links;
    size_t link_count;
    ASTNode** detaches;
    size_t detach_count;
    ASTNode** unlinks;
    size_t unlink_count;
    ASTNode** refreshes;
    size_t refresh_count;
    ASTNode** maintained_effects;
    size_t maintained_effect_count;
    ASTNode** maintained_relations;
    size_t maintained_relation_count;
    ASTNode** maintained_states;
    size_t maintained_state_count;
    ASTNode** authorities;
    size_t authority_count;
    ASTNode** states;
    size_t state_count;
    ASTNode** shared_fields;
    size_t shared_count;
    ASTNode** methods;
    size_t method_count;
    StructuredComment* doc_comment;
} ASTZoneDeclData;

typedef struct {
    char* slot_name;
    ASTNode* type;
    bool is_subject;
    bool is_vessel;
    bool is_tobject;
    bool is_binding;
    ASTNode* initializer;
} ASTDomainSlotData;

typedef struct {
    char* slot_name;
    char* layer_type;
    bool is_relation;
    bool is_pool;
    int pool_capacity;
} ASTZoneLayerSlotData;

typedef struct {
    char* effect_slot_name;
    char* target_slot_name;
    char* state_name;
    char* participant_slot_name;
} ASTZoneApplyData;

typedef struct {
    char* relation_slot_name;
    char* left_slot_name;
    char* right_slot_name;
    char* state_name;
    char* participant_slot_name;
} ASTZoneLinkData;

typedef ASTZoneApplyData ASTZoneDetachData;
typedef ASTZoneLinkData ASTZoneUnlinkData;

typedef struct {
    char* object_slot_name;
    char* source_slot_name;
    char* participant_slot_name;
    bool requires_dto;
    bool derive_target_kind;
    char** mapped_target_fields;
    char** mapped_source_fields;
    size_t field_map_count;
} ASTZoneRefreshData;

typedef struct {
    char* effect_slot_name;
    char* target_slot_name;
    char* participant_slot_name;
} ASTZoneMaintainEffectData;

typedef struct {
    char* relation_slot_name;
    char* left_slot_name;
    char* right_slot_name;
    char* participant_slot_name;
} ASTZoneMaintainRelationData;

typedef struct {
    char* state_name;
    char* participant_slot_name;
} ASTZoneMaintainStateData;

typedef struct {
    char* subject_slot_name;
    ASTNode** required_abilities;
    size_t ability_count;
} ASTZoneAuthorityData;

typedef struct {
    char* state_name;
    bool is_relation;
    char* layer_slot_name;
    char* left_or_target_slot_name;
    char* right_slot_name;
} ASTZoneStateData;

#endif /* PERGYRA_AST_DOMAIN_DATA_H */
