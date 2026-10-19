#ifndef PGY_TRANSPILER_EXPR_STDLIB_BUILTIN_H
#define PGY_TRANSPILER_EXPR_STDLIB_BUILTIN_H

#include "transpiler.h"

char *emit_call_stdlib_builtin(ASTNode *call, ASTNode *callee,
                               TranspilerCtx *ctx);

/* The inner type name of `family<T>` (an alias resolved first), or false. */
bool transpiler_resolve_unary_constructed_inner(TranspilerCtx *ctx,
                                                const char *type_name,
                                                const char *family,
                                                char *inner_buf,
                                                size_t inner_buf_size,
                                                const char **inner_out);
/* The runtime Array kernel suffix of an element type name, in `inner_buf`. */
void transpiler_array_inner_sanitize_suffix(char *inner_buf,
                                            size_t inner_buf_size,
                                            const char **inner_out);

#endif /* PGY_TRANSPILER_EXPR_STDLIB_BUILTIN_H */
