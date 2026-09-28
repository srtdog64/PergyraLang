#ifndef PGY_TRANSPILER_STATEMENT_DISPATCH_H
#define PGY_TRANSPILER_STATEMENT_DISPATCH_H

#include "transpiler.h"

/* subject_expr is the C value of the bound subject; the slot stores its
 * address (docs/206 section 1). */
bool transpiler_emit_bind_statement_parts(TranspilerCtx *ctx,
                                          const char *pvar,
                                          const char *slot_name,
                                          const ASTNode *subject,
                                          const char *subject_expr,
                                          const char *role_name);

#endif /* PGY_TRANSPILER_STATEMENT_DISPATCH_H */
