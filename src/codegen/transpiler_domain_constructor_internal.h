#ifndef PGY_TRANSPILER_DOMAIN_CONSTRUCTOR_INTERNAL_H
#define PGY_TRANSPILER_DOMAIN_CONSTRUCTOR_INTERNAL_H

#include "transpiler.h"

char *transpiler_emit_ctor_arg_with_expected_type(TranspilerCtx *ctx,
                                                  ASTNode *field_type,
                                                  const char *field_name,
                                                  ASTNode *arg);

#endif /* PGY_TRANSPILER_DOMAIN_CONSTRUCTOR_INTERNAL_H */
