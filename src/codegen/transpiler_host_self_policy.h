/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend host self-cell classification policy.
 */

#ifndef PERGYRA_TRANSPILER_HOST_SELF_POLICY_H
#define PERGYRA_TRANSPILER_HOST_SELF_POLICY_H

#include "transpiler.h"

bool transpiler_host_decl_uses_pointer_self(ASTNode *decl);
bool is_pointer_self_host_type_name(TranspilerCtx *ctx,
                                    const char *type_name);

#endif /* PERGYRA_TRANSPILER_HOST_SELF_POLICY_H */
