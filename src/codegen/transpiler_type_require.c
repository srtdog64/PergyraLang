/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend type requirement helpers.
 */

#include "transpiler_type_require.h"

#include <string.h>

#include "transpiler_context.h"
#include "transpiler_type_render.h"

bool
transpiler_require_ast_c_type_copy(TranspilerCtx *ctx,
                                   ASTNode *type_ast,
                                   const char *surface_desc,
                                   char *out,
                                   size_t out_size)
{
    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';

    if (type_ast == NULL) {
        transpiler_set_backend_error(ctx,
            "cannot emit %s in C backend: missing explicit type",
            surface_desc != NULL ? surface_desc : "declaration");
        return false;
    }

    if (!pergyra_ast_type_to_c_copy_in_ctx(ctx, type_ast, out, out_size)) {
        transpiler_set_backend_error(ctx,
            "cannot lower %s to a bounded C type buffer",
            surface_desc != NULL ? surface_desc : "declaration");
        out[0] = '\0';
        return false;
    }
    if (out[0] == '\0' || strcmp(out, "Unknown") == 0) {
        transpiler_set_backend_error(ctx,
            "cannot lower %s to a concrete C type",
            surface_desc != NULL ? surface_desc : "declaration");
        out[0] = '\0';
        return false;
    }
    return true;
}

bool
transpiler_require_type_name_c_type_copy(TranspilerCtx *ctx,
                                         const char *type_name,
                                         const char *surface_desc,
                                         char *out,
                                         size_t out_size)
{
    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';

    if (type_name == NULL || type_name[0] == '\0') {
        transpiler_set_backend_error(ctx,
            "cannot emit %s in C backend: missing concrete type name",
            surface_desc != NULL ? surface_desc : "declaration");
        return false;
    }

    if (!pergyra_type_to_c_copy(type_name, out, out_size)
        || out[0] == '\0'
        || strcmp(out, "Unknown") == 0) {
        transpiler_set_backend_error(ctx,
            "cannot lower %s type '%s' to a concrete C type",
            surface_desc != NULL ? surface_desc : "declaration",
            type_name);
        out[0] = '\0';
        return false;
    }

    return true;
}
