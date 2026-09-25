#ifndef PGY_TRANSPILER_LET_EMIT_H
#define PGY_TRANSPILER_LET_EMIT_H

#include <stdbool.h>
#include <stddef.h>

#include "transpiler.h"

/* Writes into `out` the statement that leaves the enclosing function when the
 * try-let temporary `__try_<try_id>`, of C type `operand_c_type`, holds Err.
 * A function returning the operand's Result returns the temporary; one
 * returning another Result rebuilds the error in that Result; any other
 * function panics. Returns false with a backend error set when the return
 * Result has no C rendering or the statement does not fit. */
bool transpiler_result_try_leave_stmt(TranspilerCtx *ctx, int try_id,
                                      const char *operand_c_type,
                                      char *out, size_t out_cap);

#endif /* PGY_TRANSPILER_LET_EMIT_H */
