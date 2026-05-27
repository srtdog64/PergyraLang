#ifndef PGY_TRANSPILER_GENERIC_PARAM_QUERY_H
#define PGY_TRANSPILER_GENERIC_PARAM_QUERY_H

#include "transpiler.h"

bool transpiler_func_has_generic_params(ASTNode *node);
bool transpiler_class_has_generic_params(ASTNode *node);
char *transpiler_generic_param_effective_arg_name(GenericParam *formal,
                                                  GenericParam *arg);
char *transpiler_generic_param_effective_arg_name_in_ctx(TranspilerCtx *ctx,
                                                         GenericParam *formal,
                                                         GenericParam *arg);

#endif /* PGY_TRANSPILER_GENERIC_PARAM_QUERY_H */
