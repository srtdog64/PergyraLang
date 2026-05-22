#ifndef PERGYRA_TYPE_CHECKER_OWNERSHIP_CONSUMERS_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_OWNERSHIP_CONSUMERS_INTERNAL_H

#include "type_checker_ownership_diag_internal.h"

bool
type_check_return_stmt(ASTNode *node, SemanticContext *ctx);

bool
semantic_check_assignment_borrow_rebind(ASTNode *expr,
                                        SemanticContext *ctx,
                                        Type *target_type,
                                        Type *value_type);

void
reject_borrowed_array_literal_store(ASTNode *value_expr,
                                    const Type *stored_value_type,
                                    SemanticContext *ctx);

bool
type_check_let_destructure_stmt(ASTNode *node, SemanticContext *ctx);

bool
semantic_validate_borrowed_boundary_call_argument(ASTNode *arg_expr,
                                                  SemanticContext *ctx,
                                                  ASTNode *callee_decl,
                                                  const char *display_name,
                                                  size_t arg_index,
                                                  ParamMode pmode,
                                                  Type *arg_type,
                                                  const char *local_fix_label,
                                                  bool track_borrow_provenance);

bool
semantic_check_function_call_ownership_argument(ASTNode *arg_expr,
                                                SemanticContext *ctx,
                                                const char *display_name,
                                                size_t arg_index,
                                                Type *param_type,
                                                Type *arg_type,
                                                OwnershipTypeClass param_ownership,
                                                OwnershipTypeClass arg_ownership,
                                                bool *handled_out);

void
semantic_check_param_summary_escapes(ASTNode *node,
                                     size_t param_count,
                                     Type **param_types,
                                     SemanticContext *ctx);

#endif
