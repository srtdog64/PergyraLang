/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR SSA name rendering helpers.
 */

#ifndef PERGYRA_TRANSPILER_MIR_SSA_NAMES_H
#define PERGYRA_TRANSPILER_MIR_SSA_NAMES_H

#include "transpiler.h"

const char *transpiler_resolve_active_ssa_name(const TranspilerCtx *ctx,
                                               const char *base_name);
char *transpiler_make_c_ssa_name(TranspilerCtx *ctx, const char *versioned_name);
bool transpiler_is_implicit_field(TranspilerCtx *ctx, const char *base_name);
char *transpiler_render_ssa_name(TranspilerCtx *ctx, const char *versioned_name);

#endif /* PERGYRA_TRANSPILER_MIR_SSA_NAMES_H */
