#ifndef PGY_TRANSPILER_CALL_SUBJECT_ARG_POLICY_H
#define PGY_TRANSPILER_CALL_SUBJECT_ARG_POLICY_H

#include <stdbool.h>

#include "transpiler.h"

bool transpiler_call_arg_needs_subject_address(TranspilerCtx *ctx,
                                               FuncParam *param,
                                               ASTNode *intent_param_type);
bool transpiler_call_arg_is_subject_ref(TranspilerCtx *ctx,
                                        ASTNode *arg_node);

#endif /* PGY_TRANSPILER_CALL_SUBJECT_ARG_POLICY_H */
