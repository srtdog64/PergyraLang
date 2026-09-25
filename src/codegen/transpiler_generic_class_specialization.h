#ifndef PGY_TRANSPILER_GENERIC_CLASS_SPECIALIZATION_H
#define PGY_TRANSPILER_GENERIC_CLASS_SPECIALIZATION_H

#include "../parser/ast.h"
#include "transpiler.h"

/* The specialization's declaration (struct layout, runtime rows and method
 * prototypes) goes into `decls`, the declaration stream of the request, so
 * it precedes the prototype or layout that named the type. Method bodies go
 * into ctx->helpers. */
const char *ensure_generic_class_specialization(TranspilerCtx *ctx,
                                                CodeBuf *decls,
                                                ASTNode *class_decl,
                                                ASTNode *ann);

/* Map a monomorphized specialization name such as Pair_Int back to the
 * base generic class declaration (the decl for the Pair template). Returns
 * NULL when the name is not a registered generic class specialization. */
ASTNode *transpiler_generic_class_spec_base_decl(const TranspilerCtx *ctx,
                                                 const char *specialized_name);

#endif /* PGY_TRANSPILER_GENERIC_CLASS_SPECIALIZATION_H */
