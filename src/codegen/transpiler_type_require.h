/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend type requirement helpers.
 */

#ifndef PERGYRA_TRANSPILER_TYPE_REQUIRE_H
#define PERGYRA_TRANSPILER_TYPE_REQUIRE_H

#include "transpiler.h"

const char *transpiler_require_ast_c_type(TranspilerCtx *ctx,
                                          ASTNode *type_ast,
                                          const char *surface_desc);
bool transpiler_require_ast_c_type_copy(TranspilerCtx *ctx,
                                         ASTNode *type_ast,
                                         const char *surface_desc,
                                         char *out,
                                         size_t out_size);
const char *transpiler_require_type_name_c_type(TranspilerCtx *ctx,
                                                const char *type_name,
                                                const char *surface_desc);
bool transpiler_require_type_name_c_type_copy(TranspilerCtx *ctx,
                                              const char *type_name,
                                              const char *surface_desc,
                                              char *out,
                                              size_t out_size);

#endif /* PERGYRA_TRANSPILER_TYPE_REQUIRE_H */
