/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend type-alias declaration emitter.
 */

#ifndef PERGYRA_TRANSPILER_TYPE_ALIAS_H
#define PERGYRA_TRANSPILER_TYPE_ALIAS_H

#include "transpiler.h"

void emit_type_alias_decl(ASTNode *node, TranspilerCtx *ctx);

#endif /* PERGYRA_TRANSPILER_TYPE_ALIAS_H */
