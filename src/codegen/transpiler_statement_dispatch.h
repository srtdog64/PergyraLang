#ifndef PGY_TRANSPILER_STATEMENT_DISPATCH_H
#define PGY_TRANSPILER_STATEMENT_DISPATCH_H

#include "transpiler.h"

bool transpiler_emit_bind_statement_parts(TranspilerCtx *ctx,
                                          const char *pvar,
                                          const char *slot_name,
                                          const char *role_name);

#endif /* PGY_TRANSPILER_STATEMENT_DISPATCH_H */
