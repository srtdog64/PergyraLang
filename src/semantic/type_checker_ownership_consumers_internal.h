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

void
semantic_check_param_summary_escapes(ASTNode *node,
                                     size_t param_count,
                                     Type **param_types,
                                     SemanticContext *ctx);

#endif
