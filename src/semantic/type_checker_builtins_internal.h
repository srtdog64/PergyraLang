/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Cross-TU helper declarations for the builtins dispatcher family.
 * Declarations here back defining .inc fragments that are compiled as part
 * of the type_checker_builtins.c TU but are called from sibling TUs
 * (e.g. type_checker_builtins_stdlib_body.c).
 */

#ifndef PERGYRA_TYPE_CHECKER_BUILTINS_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_BUILTINS_INTERNAL_H

#include <stdbool.h>

#include "../parser/ast.h"
#include "type_system.h"
#include "type_checker.h"

bool type_is_future_like(const Type *type);

Type *type_check_channel_send_builtin(ASTNode *expr, const char *name,
                                      bool has_timeout, bool detailed_status,
                                      SemanticContext *ctx);

Type *type_check_channel_recv_builtin(ASTNode *expr, const char *name,
                                      bool has_timeout, SemanticContext *ctx);

Type *type_check_claim_device_slot(ASTNode *call, SemanticContext *ctx);

Type *type_check_device_handle_arg(ASTNode *expr, SemanticContext *ctx,
                                   const char *builtin_name,
                                   bool allow_released);

void reject_borrowed_boundary_container_store(ASTNode *value_expr,
                                              const Type *stored_value_type,
                                              const char *container_kind,
                                              const char *container_name,
                                              SemanticContext *ctx);

#endif /* PERGYRA_TYPE_CHECKER_BUILTINS_INTERNAL_H */
