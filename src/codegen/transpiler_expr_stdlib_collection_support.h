#ifndef PGY_TRANSPILER_EXPR_STDLIB_COLLECTION_SUPPORT_H
#define PGY_TRANSPILER_EXPR_STDLIB_COLLECTION_SUPPORT_H

#include "transpiler.h"

const char *transpiler_expr_infer_type_name(TranspilerCtx *ctx,
                                            ASTNode *expr);
void transpiler_collection_ensure_specialization(TranspilerCtx *ctx,
                                                 const char *kind,
                                                 const char *inner_type);

#endif /* PGY_TRANSPILER_EXPR_STDLIB_COLLECTION_SUPPORT_H */
