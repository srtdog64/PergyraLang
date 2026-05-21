/*
 * Copyright (c) 2026 Pergyra Language Project
 * Shared AST compatibility views for hosted declarations.
 */

#ifndef PGY_HOST_DECL_COMPAT_H
#define PGY_HOST_DECL_COMPAT_H

#include <stdbool.h>
#include <stddef.h>

#include "../parser/ast.h"

typedef struct
{
    ASTNode **methods;
    size_t   count;
} PgyHostMethodCompatView;

typedef struct
{
    ASTNode **fields;
    size_t   count;
} PgyHostSharedFieldsCompatView;

const ASTNodeType *pgy_host_decl_compat_types(size_t *count_out);
const ASTNodeType *pgy_host_decl_compat_nominal_lookup_types(
    size_t *count_out);
const ASTNodeType *pgy_host_decl_compat_constructor_domain_types(
    size_t *count_out);
bool pgy_host_decl_compat_is_type(ASTNodeType decl_type);
const char *pgy_host_decl_compat_name(ASTNode *decl);
bool pgy_host_decl_compat_uses_pointer_self(ASTNode *decl);
bool pgy_host_decl_compat_has_projection_ready_flag(ASTNode *decl);
PgyHostMethodCompatView pgy_host_method_compat_view_from_decl(
    ASTNode *decl,
    bool require_role_method_total);
PgyHostSharedFieldsCompatView pgy_host_shared_fields_compat_view_from_decl(
    ASTNode *decl);

#endif /* PGY_HOST_DECL_COMPAT_H */
