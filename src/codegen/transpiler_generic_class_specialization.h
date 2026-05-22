#ifndef PGY_TRANSPILER_GENERIC_CLASS_SPECIALIZATION_H
#define PGY_TRANSPILER_GENERIC_CLASS_SPECIALIZATION_H

#include "../parser/ast.h"
#include "transpiler.h"

const char *ensure_generic_class_specialization(TranspilerCtx *ctx,
                                                ASTNode *class_decl,
                                                ASTNode *ann);

#endif /* PGY_TRANSPILER_GENERIC_CLASS_SPECIALIZATION_H */
