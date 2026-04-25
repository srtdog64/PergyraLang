/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend nominal member and receiver type lookup.
 */

#ifndef PERGYRA_TRANSPILER_NOMINAL_H
#define PERGYRA_TRANSPILER_NOMINAL_H

#include "transpiler.h"

const char *transpiler_current_field_type_name(TranspilerCtx *ctx,
                                               const char *field_name);
const char *transpiler_lookup_nominal_host_member_type_name(
    TranspilerCtx *ctx, const char *host_type_name, const char *member_name);
const char *transpiler_resolve_nominal_host_expr_type_name(TranspilerCtx *ctx,
                                                           ASTNode *expr);

#endif /* PERGYRA_TRANSPILER_NOMINAL_H */
