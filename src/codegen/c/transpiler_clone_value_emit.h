#ifndef PGY_TRANSPILER_CLONE_VALUE_EMIT_H
#define PGY_TRANSPILER_CLONE_VALUE_EMIT_H

#include "../transpiler.h"

/* C text of `Clone` of `value`, C expression text of a value of Pergyra type
 * `type_name`. Every String, Array, Option payload and non-generic struct
 * field it owns is copied into new storage
 * (2026-10-19_struct_clone_is_deep); any other part is copied by value. The
 * checker's result relation treats only fully copied types as independent of
 * the argument. NULL after a backend error. */
char *transpiler_emit_clone_value(TranspilerCtx *ctx, const ASTNode *site, const char *type_name,
                                  const char *value);

#endif /* PGY_TRANSPILER_CLONE_VALUE_EMIT_H */
