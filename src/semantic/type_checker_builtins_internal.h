/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Cross-TU helper declarations for the builtins dispatcher family.
 * Declarations here expose helpers owned by focused builtins translation
 * units so sibling TUs can share validation without reintroducing .inc seams.
 */

#ifndef PERGYRA_TYPE_CHECKER_BUILTINS_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_BUILTINS_INTERNAL_H

#include <stdbool.h>

#include "../parser/ast.h"
#include "builtin_kind.h"
#include "type_system.h"
#include "type_checker_builtins_query.h"
#include "type_checker_builtins_slotops.h"

typedef struct SemanticContext SemanticContext;
typedef struct Symbol Symbol;

bool type_is_future_like(const Type *type);

Type *type_check_channel_send_builtin(ASTNode *expr, const char *name,
                                      bool has_timeout, bool detailed_status,
                                      SemanticContext *ctx);

Type *type_check_channel_recv_builtin(ASTNode *expr, const char *name,
                                      bool has_timeout, SemanticContext *ctx);

Type *type_check_channel_close_builtin(ASTNode *expr, SemanticContext *ctx);

Type *type_check_channel_state_builtin(ASTNode *expr, const char *name,
                                       SemanticContext *ctx,
                                       bool *handled_out);

Type *type_check_state_tool_builtin(ASTNode *expr, const char *name,
                                    SemanticContext *ctx,
                                    bool *handled_out);

bool type_check_cancel_rejects_payload(ASTNode *site, Type *task_type,
                                       SemanticContext *ctx);

Type *type_check_stdlib_scalar_call(ASTNode *expr, const char *name,
                                    SemanticContext *ctx, bool *handled_out);
Type *type_check_stdlib_map_call(ASTNode *expr, const char *name,
                                 SemanticContext *ctx, bool *handled_out);
Type *type_check_stdlib_collection_call(ASTNode *expr, const char *name,
                                        SemanticContext *ctx,
                                        bool *handled_out);
Type *type_check_stdlib_variant_builtin_call(ASTNode *expr, const char *name,
                                             SemanticContext *ctx,
                                             bool *handled_out);
Type *type_check_stdlib_channel_transport_call(ASTNode *expr, const char *name,
                                               SemanticContext *ctx,
                                               bool *handled_out);
Type *type_check_intent_observability_builtin(ASTNode *call, BuiltinKind kind,
                                              SemanticContext *ctx,
                                              bool *handled_out);
Type *type_check_nominal_ownership_builtin(ASTNode *call, BuiltinKind kind,
                                           SemanticContext *ctx,
                                           bool *handled_out);
bool builtin_validate_secure_token_arg(ASTNode *token_arg,
                                       Symbol *slot_sym,
                                       Type *slot_type,
                                       SemanticContext *ctx);

void reject_borrowed_boundary_container_store(ASTNode *value_expr,
                                              const Type *stored_value_type,
                                              const char *container_kind,
                                              const char *container_name,
                                              SemanticContext *ctx);

#endif /* PERGYRA_TYPE_CHECKER_BUILTINS_INTERNAL_H */
