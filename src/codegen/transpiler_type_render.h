/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend type rendering seam.
 */

#ifndef PERGYRA_TRANSPILER_TYPE_RENDER_H
#define PERGYRA_TRANSPILER_TYPE_RENDER_H

#include "transpiler.h"

const char *transpiler_render_type_name_local(TranspilerCtx *ctx,
                                              ASTNode *type_node);

#endif /* PERGYRA_TRANSPILER_TYPE_RENDER_H */
