/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type checker visibility / access-control helpers.
 *
 * Exported by type_checker_visibility.c so other semantic owner modules can
 * reference these checks without hidden include-order coupling.
 */

#ifndef PGY_TYPE_CHECKER_VISIBILITY_H
#define PGY_TYPE_CHECKER_VISIBILITY_H

#include <stdbool.h>

#include "type_checker_internal.h"

/* True if `decl` (an AST_CLASS_DECL) names the same nominal type as
 * `object_type`'s runtime class. */
bool nominal_decl_matches_runtime_type(ASTNode *decl, Type *object_type);

/* True if a private member of `decl` is reachable from the current
 * host nominal scope in `ctx`.  Requires nominal_decl_matches_runtime_type. */
bool private_member_access_allowed(ASTNode *decl,
                                   Type *object_type,
                                   SemanticContext *ctx);

/* True if both module paths are non-NULL and refer to the same module. */
bool same_module_origin(const char *left, const char *right);

/* True if a member access on `decl` would cross a module boundary
 * relative to the current scope. */
bool cross_module_member_access(ASTNode *decl, SemanticContext *ctx);

/* Member-access policy gate: combines public/private/protected rules
 * with module boundary checks. */
bool explicit_member_access_allowed(ASTNode *decl,
                                    Type *object_type,
                                    AccessModifier access,
                                    bool has_explicit_access,
                                    SemanticContext *ctx);

/* True if a type/declaration reference at `site` may resolve to `decl`.
 * Honors `is_exported`, ability access modifier, and cross-module rules. */
bool explicit_type_reference_allowed(ASTNode *decl,
                                     const ASTNode *site,
                                     SemanticContext *ctx);

#endif /* PGY_TYPE_CHECKER_VISIBILITY_H */
