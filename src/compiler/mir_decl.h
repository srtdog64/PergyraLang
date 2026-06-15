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

typedef struct
{
    const char *owner_name;
    const char *name;
    FuncParam **params;
    char      **param_type_names;
    size_t      param_count;
    ASTNode    *return_type;
    char       *return_type_name;
    bool        is_async;
    bool        is_action_like;
    const char *within_zone;
    const char *causes_effect;
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
    char    *base_name;
    size_t   actual_arg_count;
    char   **actual_arg_type_names;
} MIRAbilityRef;

typedef struct
{
    const char      *owner_name;
    const char      *name;
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
} MIRDeclFieldClaim;

typedef struct
{
    /* Source compatibility/provenance only; generic inventory lives below. */
    GenericParam *source_param;
    const char   *name;
    ASTNode      *bound_ast;
    ASTNode      *default_arg_ast;
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
    const char  *name;
    char        *type_alias_target_type_name;
    size_t       generic_param_count;
    MIRDeclGenericParam *generic_metadata;
    size_t       generic_metadata_count;
    size_t       method_count;
    MIRDeclMethod *method_metadata;
    size_t       method_metadata_count;
    size_t       field_count;
    MIRDeclField *field_metadata;
    size_t       field_metadata_count;
    size_t       field_claim_count;
    MIRDeclFieldClaim *field_claim_metadata;
    size_t       field_claim_metadata_count;
    size_t       variant_count;
    MIRDeclEnumVariant *variant_metadata;
    size_t       variant_metadata_count;
    NominalDeclKind nominal_kind;
    bool         uses_pointer_self;
} MIRDeclHeader;

typedef struct
{
    const MIRDeclHeader *headers;
    size_t               count;
} MIRDeclHeaderInventory;

#endif /* PERGYRA_MIR_DECL_H */
