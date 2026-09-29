#ifndef PGY_TRANSPILER_MANGLED_NAME_H
#define PGY_TRANSPILER_MANGLED_NAME_H

#include "transpiler.h"

void append_mangled_type_name(CodeBuf *buf, const char *type_name);

/* PP-068: native C owns the translation-unit identifier of every non-extern
 * Pergyra function and intent, `pgy_u_<name>`. The generated C includes host
 * headers (windows.h through the runtime, unistd.h, libc) that declare
 * ordinary API names in the same C namespace, and PascalCase Pergyra names
 * meet Win32 functions such as Escape, Rectangle and Sleep. Externs keep
 * their exact host ABI spelling and never reach this owner. */
const char *transpiler_c_user_callable_symbol(TranspilerCtx *ctx,
                                              const char *name);

/* The owned symbol when `name`, read as an expression, names a non-extern
 * user function or intent; NULL for every other identifier. */
const char *transpiler_c_user_callable_reference_symbol(TranspilerCtx *ctx,
                                                        const char *name);

#endif /* PGY_TRANSPILER_MANGLED_NAME_H */
