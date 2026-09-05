#ifndef PERGYRA_MIR_DECL_H
#define PERGYRA_MIR_DECL_H

/*
 * Declaration inventory IR.
 *
 * MIR-owned structured records for declaration kinds (methods, fields,
 * generic parameters, and the per-declaration header). Split out of mir.h so
 * the declaration IR has its own home to grow as backend consumers migrate
 * off the AST-carried inventory. Semantic facts live in the structured
 * metadata below.
 */

#include <stdbool.h>
#include <stddef.h>

#include "../parser/ast.h"
#include "mir_abi.h"

typedef struct
{
    char    *base_name;
    size_t   actual_arg_count;
    char   **actual_arg_type_names;
} MIRAbilityRef;

typedef struct
{
    const char *owner_name;
    const char *name;
    uint32_t    source_syntax_id;
    FuncParam **params;
    char      **param_type_names;
    size_t      param_count;
    ASTNode    *return_type;
    char       *return_type_name;
    bool        is_async;
    bool        is_action_like;
    MIRAbilityRef *required_ability_refs;
    size_t      required_ability_ref_count;
    const char *within_zone;
    const char *causes_effect;
    char      **authorized_by_names;
    size_t      authorized_by_count;
    bool        has_caps_clause;
    uint32_t    declared_capabilities;
    bool        has_effects_clause;
    uint32_t    declared_effects;
    bool        has_routine;
    size_t      routine_index;
    char      **projection_write_root_names;
    char      **projection_write_member_names;
    size_t      projection_write_count;
    char      **projection_call_receiver_names;
    char      **projection_call_method_names;
    size_t      projection_call_count;
} MIRDeclMethod;

typedef enum
{
    MIR_DECL_FIELD_UNKNOWN,
    MIR_DECL_FIELD_CLASS,
    MIR_DECL_FIELD_SHARED,
    MIR_DECL_FIELD_ROLE_SLOT,
    MIR_DECL_FIELD_ROSTER_SLOT,
    MIR_DECL_FIELD_WORLD_ROSTER_SLOT,
    MIR_DECL_FIELD_WORLD_ZONE_SLOT,
    MIR_DECL_FIELD_DOMAIN_SLOT,
    MIR_DECL_FIELD_ZONE_LAYER_SLOT
} MIRDeclFieldKind;

typedef struct
{
    const char    *owner_name;
    MIRAbilityRef  ability_ref;
    size_t         method_start_index;
    size_t         method_count;
} MIRDeclRoleImpl;

typedef struct
{
    const char *owner_name;
    const char *role_name;
} MIRDeclRoleInclude;

typedef struct
{
    const char      *owner_name;
    const char      *name;
    uint32_t         source_syntax_id;
    ASTNode         *type;
    ASTNode         *initializer;
    char            *type_name;
    MIRDeclFieldKind kind;
    bool             is_dynamic;
    bool             is_subject_like;
    bool             is_tobject_like;
    bool             is_binding_like;
    bool             is_relation_layer;
    bool             is_pool_layer;
    int              pool_capacity;
    MIRAbilityRef   *required_ability_refs;
    size_t           required_ability_ref_count;
} MIRDeclField;

typedef struct
{
    const char *owner_name;
    const char *slot_name;
    const char *token_name;
    char       *inner_type_name;
    bool        is_secure;
    MIRResourceRuntimeRow runtime_call_abi;
    const MIRTypeLayout   *type_layout;
    uint32_t               abi_layout_id;
    bool                   runtime_call_abi_present;
} MIRDeclFieldClaim;

typedef struct
{
    const char    *owner_name;
    const char    *subject_slot_name;
    MIRAbilityRef *required_ability_refs;
    size_t         required_ability_ref_count;
} MIRDeclZoneAuthority;

typedef struct
{
    const char *target_field_name;
    const char *source_field_name;
} MIRDeclZoneRefreshFieldMap;

typedef struct
{
    const char *owner_name;
    const char *object_slot_name;
    const char *source_slot_name;
    const char *participant_slot_name;
    bool        requires_dto;
    bool        derives_target_kind;
    MIRDeclZoneRefreshFieldMap *field_maps;
    size_t      field_map_count;
} MIRDeclZoneRefresh;

typedef struct
{
    const char *owner_name;
    const char *name;
    const char *layer_slot_name;
    const char *left_or_target_slot_name;
    const char *right_slot_name;
    bool        is_relation;
} MIRDeclZoneState;

/* MIR-owned world derived-state row. This is intentionally separate from
 * MIRDeclZoneState: world states carry source composition and input identity,
 * while zone states carry layer/left/right topology. */
typedef struct
{
    const char          *owner_name;
    char                *name;
    char                *zone_slot_name;
    WorldStateSourceKind source_kind;
    char                *detail_name;
    char               **input_names;
    size_t               input_count;
} MIRDeclWorldState;

typedef enum
{
    MIR_DECL_WORLD_DIRECTIVE_ACTIVATE,
    MIR_DECL_WORLD_DIRECTIVE_MAINTAIN,
    MIR_DECL_WORLD_DIRECTIVE_DEACTIVATE
} MIRDeclWorldDirectiveKind;

/* MIR-owned world command row. A directive names either a concrete zone
 * slot or a derived world-state name; backend consumers must not reopen the
 * AST directive payload after MIR admission. */
typedef struct
{
    const char                *owner_name;
    MIRDeclWorldDirectiveKind  kind;
    char                      *zone_slot_name;
    char                      *state_name;
} MIRDeclWorldDirective;

typedef struct
{
    const char   *name;
    char         *bound_type_name;
    char         *default_arg_type_name;
} MIRDeclGenericParam;

typedef struct
{
    /* name is provenance-backed; param_type_names are MIR-owned captures. */
    const char  *name;
    size_t       param_count;
    const char **param_type_names;
} MIRDeclEnumVariant;

typedef struct
{
    ASTNodeType  ast_type;
    /* Stable source identity of the declaration itself.  Consumers that
     * cross-seal nominal or enum facts must never recover this identity from
     * a declaration name or inventory position. */
    uint32_t     source_syntax_id;
    /* Borrowed parser provenance. The AST owns this path for the full MIR
     * lifetime; the JSON wire preserves it so MIR-to-AST reconstruction does
     * not weaken module-scoped semantic admission. */
    const char  *source_module_path;
    const char  *name;
    char        *type_alias_target_type_name;
    int          intent_retry_count;
    size_t       generic_param_count;
    MIRDeclGenericParam *generic_metadata;
    size_t       generic_metadata_count;
    size_t       method_count;
    MIRDeclMethod *method_metadata;
    size_t       method_metadata_count;
    char        *role_subject_type_name;
    size_t       role_impl_count;
    MIRDeclRoleImpl *role_impl_metadata;
    size_t       role_impl_metadata_count;
    /* Override methods are role-owned methods but not ability impl rows.
     * They are appended after normal method spans. */
    size_t       role_override_method_count;
    size_t       role_include_count;
    MIRDeclRoleInclude *role_include_metadata;
    size_t       role_include_metadata_count;
    size_t       field_count;
    MIRDeclField *field_metadata;
    size_t       field_metadata_count;
    size_t       field_claim_count;
    MIRDeclFieldClaim *field_claim_metadata;
    size_t       field_claim_metadata_count;
    size_t       zone_authority_count;
    MIRDeclZoneAuthority *zone_authority_metadata;
    size_t       zone_authority_metadata_count;
    size_t       zone_refresh_count;
    MIRDeclZoneRefresh *zone_refresh_metadata;
    size_t       zone_refresh_metadata_count;
    size_t       zone_state_count;
    MIRDeclZoneState *zone_state_metadata;
    size_t       zone_state_metadata_count;
    size_t       world_state_count;
    MIRDeclWorldState *world_state_metadata;
    size_t       world_state_metadata_count;
    size_t       world_directive_count;
    MIRDeclWorldDirective *world_directive_metadata;
    size_t       world_directive_metadata_count;
    size_t       variant_count;
    MIRDeclEnumVariant *variant_metadata;
    size_t       variant_metadata_count;
    /* Event handler ABI rows. Event declarations are not routines, but
     * their ordered parameter names/types are a backend-facing contract. */
    size_t       event_param_count;
    char       **event_param_names;
    char       **event_param_type_names;
    bool         event_param_metadata_present;
    NominalDeclKind nominal_kind;
    bool         uses_pointer_self;
    bool         abi_layout_present;
    MIRTypeLayout abi_layout;
    uint32_t     abi_layout_id;
    /* Program-owned derived receipt for Option<this value struct>.  The
     * wrapper is captured beside its inner nominal row so instructions never
     * ask a backend to reconstruct tag or payload offsets from type text. */
    bool         option_abi_layout_present;
    char        *option_abi_type_name;
    MIRTypeLayout option_abi_layout;
    uint32_t     option_abi_layout_id;
} MIRDeclHeader;

typedef struct
{
    const MIRDeclHeader *headers;
    size_t               count;
} MIRDeclHeaderInventory;

#endif /* PERGYRA_MIR_DECL_H */
