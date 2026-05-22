#ifndef PGY_SRC_CODEGEN_TRANSPILER_GENERIC_SPECIALIZATION_EMIT_H
#define PGY_SRC_CODEGEN_TRANSPILER_GENERIC_SPECIALIZATION_EMIT_H

#include "transpiler.h"

const char *ensure_generic_specialization(TranspilerCtx *ctx,
                                          ASTNode *decl,
                                          ASTNode *call);

#endif /* PGY_SRC_CODEGEN_TRANSPILER_GENERIC_SPECIALIZATION_EMIT_H */
