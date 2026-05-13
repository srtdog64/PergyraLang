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

const char *
transpiler_require_ast_c_type(TranspilerCtx *ctx,
                              ASTNode *type_ast,
                              const char *surface_desc)
{
    const char *c_type;

    if (type_ast == NULL) {
        transpiler_set_backend_error(ctx,
            "cannot emit %s in C backend: missing explicit type",
            surface_desc != NULL ? surface_desc : "declaration");
        return NULL;
    }

    c_type = pergyra_ast_type_to_c(type_ast);
    if (c_type == NULL || c_type[0] == '\0' || strcmp(c_type, "Unknown") == 0) {
        transpiler_set_backend_error(ctx,
            "cannot lower %s to a concrete C type",
            surface_desc != NULL ? surface_desc : "declaration");
        return NULL;
    }

    return c_type;
}

bool
transpiler_require_ast_c_type_copy(TranspilerCtx *ctx,
                                   ASTNode *type_ast,
                                   const char *surface_desc,
                                   char *out,
                                   size_t out_size)
{
    const char *c_type;
    size_t len;

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';

    c_type = transpiler_require_ast_c_type(ctx, type_ast, surface_desc);
    if (c_type == NULL)
        return false;
    len = strlen(c_type);
    if (len >= out_size) {
        transpiler_set_backend_error(ctx,
            "cannot lower %s to a bounded C type buffer",
            surface_desc != NULL ? surface_desc : "declaration");
        return false;
    }
    memcpy(out, c_type, len + 1);
    return true;
}

const char *
transpiler_require_type_name_c_type(TranspilerCtx *ctx,
                                    const char *type_name,
                                    const char *surface_desc)
{
    static char c_type[256];
    c_type[0] = '\0';

    if (type_name == NULL || type_name[0] == '\0') {
        transpiler_set_backend_error(ctx,
            "cannot emit %s in C backend: missing concrete type name",
            surface_desc != NULL ? surface_desc : "declaration");
        return NULL;
    }

    (void)pergyra_type_to_c_copy(type_name, c_type, sizeof(c_type));
    if (c_type[0] == '\0' || strcmp(c_type, "Unknown") == 0) {
        transpiler_set_backend_error(ctx,
            "cannot lower %s type '%s' to a concrete C type",
            surface_desc != NULL ? surface_desc : "declaration",
            type_name);
        return NULL;
    }

    return c_type;
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
