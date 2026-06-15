/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend host self-cell classification policy.
 */

#include "../compiler/mir_decl_headers.h"
#include "host_decl_compat.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_inventory_view.h"

bool
transpiler_host_decl_uses_pointer_self(ASTNode *decl)
{
    return pgy_host_decl_compat_uses_pointer_self(decl);
}

bool
is_pointer_self_host_type_name(TranspilerCtx *ctx, const char *type_name)
{
    const MIRDeclHeader *header;
    ASTNode *decl;

    if (type_name == NULL)
        return false;
    header = transpiler_active_host_decl_header(ctx, type_name);
    if (header != NULL)
        return mir_decl_header_uses_pointer_self(header);
    if (transpiler_active_has_mir(ctx))
        return false;
    decl = transpiler_find_nominal_host_decl_local(ctx, type_name);
    return transpiler_host_decl_uses_pointer_self(decl);
}
