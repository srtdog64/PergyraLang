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
const char *transpiler_require_type_name_c_type(TranspilerCtx *ctx,
                                                const char *type_name,
                                                const char *surface_desc);

#endif /* PERGYRA_TRANSPILER_TYPE_REQUIRE_H */
