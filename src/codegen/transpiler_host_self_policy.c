/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend host self-cell classification policy.
 */

#include "host_decl_compat.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_host_self_policy.h"

bool
transpiler_host_decl_uses_pointer_self(ASTNode *decl)
{
    return pgy_host_decl_compat_uses_pointer_self(decl);
}

bool
is_pointer_self_host_type_name(TranspilerCtx *ctx, const char *type_name)
{
    ASTNode *decl;

    if (type_name == NULL)
        return false;
    decl = transpiler_find_nominal_host_decl_local(ctx, type_name);
    return transpiler_host_decl_uses_pointer_self(decl);
}
