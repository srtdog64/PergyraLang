#ifndef PGY_TRANSPILER_CALL_SUBJECT_ARG_POLICY_H
#define PGY_TRANSPILER_CALL_SUBJECT_ARG_POLICY_H

#include <stdbool.h>

#include "transpiler.h"

bool transpiler_call_arg_needs_subject_address(TranspilerCtx *ctx,
                                               FuncParam *param,
                                               const char *param_type_name,
                                               ASTNode *intent_param_type,
                                               const char *intent_param_type_name);
bool transpiler_call_arg_can_take_subject_address(TranspilerCtx *ctx,
                                                  ASTNode *arg_node);
bool transpiler_call_arg_is_indirect_ref(TranspilerCtx *ctx,
                                         ASTNode *arg_node);
bool transpiler_call_arg_is_carried_by_address(TranspilerCtx *ctx,
                                               ASTNode *arg_node);
/* Caller owns the returned text. Never materialize a copy of an existing
 * subject merely to obtain an address. */
char *transpiler_call_arg_address(TranspilerCtx *ctx, ASTNode *arg_node,
                                  const char *arg, const char *callee_name,
                                  size_t index);

#endif /* PGY_TRANSPILER_CALL_SUBJECT_ARG_POLICY_H */
