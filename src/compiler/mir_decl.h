#ifndef PERGYRA_MIR_DECL_H
#define PERGYRA_MIR_DECL_H

/*
 * Declaration inventory IR.
 *
 * MIR-owned structured records for declaration kinds (methods, fields,
 * generic parameters, and the per-declaration header). Split out of mir.h so
 * the declaration IR has its own home to grow as backend consumers migrate
 * off the AST-carried inventory. source_ast/* fields remain provenance only;
 * semantic facts live in the structured metadata below.
 */

#include <stdbool.h>
#include <stddef.h>

#include "../parser/ast.h"

typedef struct
{
    /* Source compatibility/provenance only; declaration inventory lives below. */
    ASTNode    *source_ast;
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
    /* Source compatibility/provenance only; consumers must use the metadata. */
    ASTNode         *source_ast;
    const char      *owner_name;
    const char      *name;
    ASTNode         *type;
    const char      *type_name;
    MIRDeclFieldKind kind;
    bool             is_dynamic;
    bool             is_subject_like;
    bool             is_tobject_like;
    bool             is_binding_like;
    bool             is_relation_layer;
    bool             is_pool_layer;
    int              pool_capacity;
} MIRDeclField;

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
    /* name/param_type_names are provenance-backed (AST-owned) like MIRDeclField;
     * the arrays are owned by the header and freed with it. */
    const char  *name;
    size_t       param_count;
    const char **param_type_names;
} MIRDeclEnumVariant;

typedef struct
{
    /* Source compatibility/provenance only; declaration inventory lives below. */
    ASTNode     *source_ast;
    ASTNodeType  ast_type;
    const char  *name;
    size_t       generic_param_count;
    MIRDeclGenericParam *generic_metadata;
    size_t       generic_metadata_count;
    size_t       method_count;
    MIRDeclMethod *method_metadata;
    size_t       method_metadata_count;
    size_t       field_count;
    MIRDeclField *field_metadata;
    size_t       field_metadata_count;
    size_t       variant_count;
    MIRDeclEnumVariant *variant_metadata;
    size_t       variant_metadata_count;
    bool         uses_pointer_self;
} MIRDeclHeader;

typedef struct
{
    const MIRDeclHeader *headers;
    size_t               count;
} MIRDeclHeaderInventory;

#endif /* PERGYRA_MIR_DECL_H */

