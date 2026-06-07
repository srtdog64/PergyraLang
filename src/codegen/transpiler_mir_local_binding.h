/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR local-binding discovery helpers.
 */

#ifndef PERGYRA_TRANSPILER_MIR_LOCAL_BINDING_H
#define PERGYRA_TRANSPILER_MIR_LOCAL_BINDING_H

#include "transpiler.h"
#include "transpiler_mir_ssa_map.h"

bool transpiler_has_explicit_local_binding(const ASTNode *func_decl,
                                           const char *base_name);
void transpiler_register_with_alias_bindings_in_block(
    TranspilerSSANameMap *ssa_map,
    ASTNode *body);
void transpiler_register_ast_compat_local_bindings_in_block(
    TranspilerCtx *ctx,
    const ASTNode *func_decl,
    ASTNode *body);

#endif /* PERGYRA_TRANSPILER_MIR_LOCAL_BINDING_H */
