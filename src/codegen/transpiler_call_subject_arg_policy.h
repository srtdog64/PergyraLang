#ifndef PGY_TRANSPILER_CALL_SUBJECT_ARG_POLICY_H
#define PGY_TRANSPILER_CALL_SUBJECT_ARG_POLICY_H

#include <stdbool.h>

#include "transpiler.h"

bool transpiler_call_arg_needs_subject_address(TranspilerCtx *ctx,
                                               FuncParam *param,
                                               ASTNode *intent_param_type,
                                               const char *intent_param_type_name);
bool transpiler_call_arg_can_take_subject_address(ASTNode *arg_node);
bool transpiler_call_arg_is_subject_ref(TranspilerCtx *ctx,
                                        ASTNode *arg_node);

#endif /* PGY_TRANSPILER_CALL_SUBJECT_ARG_POLICY_H */
