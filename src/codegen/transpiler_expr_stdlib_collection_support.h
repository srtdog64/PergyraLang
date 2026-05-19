#ifndef PGY_TRANSPILER_EXPR_STDLIB_COLLECTION_SUPPORT_H
#define PGY_TRANSPILER_EXPR_STDLIB_COLLECTION_SUPPORT_H

#include <stdbool.h>
#include <stddef.h>

#include "transpiler.h"

const char *transpiler_expr_infer_type_name(TranspilerCtx *ctx,
                                            ASTNode *expr);
void transpiler_collection_ensure_specialization(TranspilerCtx *ctx,
                                                 const char *kind,
                                                 const char *inner_type);
bool transpiler_require_hashmap_type(TranspilerCtx *ctx,
                                     const char *map_type,
                                     const char *operation,
                                     char *key_buf,
                                     size_t key_buf_size,
                                     char *value_buf,
                                     size_t value_buf_size,
                                     const char **key_out,
                                     const char **value_out);
bool transpiler_require_unary_collection_type(TranspilerCtx *ctx,
                                              const char *type_name,
                                              const char *family,
                                              const char *operation,
                                              char *inner_buf,
                                              size_t inner_buf_size,
                                              const char **inner_out);

#endif /* PGY_TRANSPILER_EXPR_STDLIB_COLLECTION_SUPPORT_H */
