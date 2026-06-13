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
    size_t role_capacity;
    ASTNode** shared_fields;
    size_t shared_count;
    size_t shared_capacity;
    ASTNode** methods;
    size_t method_count;
    size_t method_capacity;
    ASTNode* extends;
    GenericParams* generic_params;
    WhereClause* where_clause;
    StructuredComment* doc_comment;
} ASTPartyDeclData;

typedef struct {
    char* slot_name;
    ASTNode** required_abilities;
    size_t ability_count;
    size_t ability_capacity;
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
    size_t party_capacity;
    ASTNode** shared_fields;
    size_t shared_count;
    size_t shared_capacity;
    ASTNode** methods;
    size_t method_count;
    size_t method_capacity;
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
    size_t roster_capacity;
    ASTNode** zones;
    size_t zone_count;
    size_t zone_capacity;
    ASTNode** shared_fields;
    size_t shared_count;
    size_t shared_capacity;
    ASTNode** methods;
    size_t method_count;
    size_t method_capacity;
    ASTNode** activations;
    size_t activate_count;
    size_t activate_capacity;
    ASTNode** deactivations;
    size_t deactivate_count;
    size_t deactivate_capacity;
    ASTNode** maintained_zones;
    size_t maintained_zone_count;
    size_t maintained_zone_capacity;
    ASTNode** states;
    size_t state_count;
    size_t state_capacity;
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
    size_t involve_capacity;
    ASTNode** values;
    size_t value_count;
    size_t value_capacity;
    ASTNode** bindings;
    size_t binding_count;
    size_t binding_capacity;
    ASTNode** steps;
    size_t step_count;
    size_t step_capacity;
    bool is_concurrent;
    IntentRollbackPolicy rollback_policy;
    ASTNode* priority_expr;
    ASTNode* success_expr;
    ASTNode* failure_expr;
    StructuredComment* doc_comment;
    char** default_who_names;
    size_t default_who_count;
    size_t default_who_capacity;
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
    size_t who_capacity;
    ASTNode** on_exprs;
    size_t on_expr_count;
    size_t on_expr_capacity;
    ASTNode** compensate_exprs;
    size_t compensate_expr_count;
    size_t compensate_expr_capacity;
    ASTNode* pre_expr;
    ASTNode* guard_expr;
    ASTNode* post_expr;
    ASTNode* invariant_expr;
    ASTNode** required_abilities;
    size_t required_ability_count;
    size_t required_ability_capacity;
    char* causes_effect;
    char** authorized_by;
    size_t authorized_by_count;
    size_t authorized_by_capacity;
    ASTNode* expect_expr;
    bool inherited_who_from_intent;
    bool derived_who_from_on_receiver;
    bool derived_who_from_single_participant;
    bool inherited_where_from_intent;
    bool inherited_who_from_action;
    bool inherited_where_from_action;
    bool inherited_requires_from_action;
    bool inherited_causes_from_action;
    bool inherited_authorized_by_from_action;
    /* Legacy schema field only. Active beta semantics must not derive
       approval from local `who`; see intent_compression_contract_smoke.sh. */
    bool derived_authorized_by_from_zone;
    bool derived_where_from_using;
    bool derived_where_from_transfer;
    bool derived_using_from_transfer;
    bool derived_using_from_where;
} ASTIntentStepData;

typedef struct {
    char* name;
    ASTNode** slots;
    size_t slot_count;
    size_t slot_capacity;
    ASTNode** refreshes;
    size_t refresh_count;
    size_t refresh_capacity;
    ASTNode** shared_fields;
    size_t shared_count;
    size_t shared_capacity;
    ASTNode** methods;
    size_t method_count;
    size_t method_capacity;
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
    size_t slot_capacity;
    ASTNode** refreshes;
    size_t refresh_count;
    size_t refresh_capacity;
    ASTNode** shared_fields;
    size_t shared_count;
    size_t shared_capacity;
    ASTNode** methods;
    size_t method_count;
    size_t method_capacity;
    StructuredComment* doc_comment;
} ASTEffectDeclData;

typedef struct {
    char* name;
    ASTNode** slots;
    size_t slot_count;
    size_t slot_capacity;
    ASTNode** layer_slots;
    size_t layer_slot_count;
    size_t layer_slot_capacity;
    ASTNode** applies;
    size_t apply_count;
    size_t apply_capacity;
    ASTNode** links;
    size_t link_count;
    size_t link_capacity;
    ASTNode** detaches;
    size_t detach_count;
    size_t detach_capacity;
    ASTNode** unlinks;
    size_t unlink_count;
    size_t unlink_capacity;
    ASTNode** refreshes;
    size_t refresh_count;
    size_t refresh_capacity;
    ASTNode** maintained_effects;
    size_t maintained_effect_count;
    size_t maintained_effect_capacity;
    ASTNode** maintained_relations;
    size_t maintained_relation_count;
    size_t maintained_relation_capacity;
    ASTNode** maintained_states;
    size_t maintained_state_count;
    size_t maintained_state_capacity;
    ASTNode** authorities;
    size_t authority_count;
    size_t authority_capacity;
    ASTNode** states;
    size_t state_count;
    size_t state_capacity;
    ASTNode** shared_fields;
    size_t shared_count;
    size_t shared_capacity;
    ASTNode** methods;
    size_t method_count;
    size_t method_capacity;
    bool forbids_unsafe;
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
    size_t ability_capacity;
} ASTZoneAuthorityData;

typedef struct {
    char* state_name;
    bool is_relation;
    char* layer_slot_name;
    char* left_or_target_slot_name;
    char* right_slot_name;
} ASTZoneStateData;

#endif /* PERGYRA_AST_DOMAIN_DATA_H */
