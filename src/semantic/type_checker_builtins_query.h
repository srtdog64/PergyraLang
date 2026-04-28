/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Builtin query and predicate helper declarations.
 */

#ifndef PERGYRA_TYPE_CHECKER_BUILTINS_QUERY_H
#define PERGYRA_TYPE_CHECKER_BUILTINS_QUERY_H

#include <stdbool.h>
#include <stddef.h>

#include "../parser/ast.h"
#include "type_checker.h"
#include "type_system.h"

bool check_call_arity(ASTNode *expr, size_t expected, const char *name,
                      SemanticContext *ctx);

void reject_borrowed_boundary_container_store(ASTNode *value_expr,
                                              const Type *stored_value_type,
                                              const char *container_kind,
                                              const char *container_name,
                                              SemanticContext *ctx);

Type *type_check_has_projection(ASTNode *call, SemanticContext *ctx);
Type *type_check_has_layer(ASTNode *call, SemanticContext *ctx);
Type *type_check_has_state(ASTNode *call, SemanticContext *ctx);
Type *type_check_has_zone(ASTNode *call, SemanticContext *ctx);
Type *type_check_has_zone_projection_builtin(ASTNode *call,
                                             SemanticContext *ctx);
Type *type_check_has_zone_layer_builtin(ASTNode *call, SemanticContext *ctx);
Type *type_check_has_zone_state_builtin(ASTNode *call, SemanticContext *ctx);

#endif /* PERGYRA_TYPE_CHECKER_BUILTINS_QUERY_H */
