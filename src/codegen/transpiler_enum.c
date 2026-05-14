/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend enum lowering helpers.
 */

#include <stdio.h>
#include <string.h>

#include "transpiler_enum.h"

bool
lookup_enum_variant_qualified_name_copy(TranspilerCtx *ctx,
                                        const char *variant_name,
                                        char *out,
                                        size_t out_size)
{
    ASTNode **types = NULL;
    size_t type_count = 0;

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';
    if (ctx == NULL || variant_name == NULL)
        return false;

    transpiler_active_inventory(ctx, AST_ENUM_DECL, &types, &type_count);
    if (types == NULL)
        return false;

    for (size_t i = 0; i < type_count; i++) {
        ASTNode *stmt = types[i];
        size_t variant_count = 0;
        char **variants;
        if (stmt == NULL || stmt->type != AST_ENUM_DECL)
            continue;
        variants = ast_enum_variants(stmt, &variant_count);
        for (size_t j = 0; j < variant_count; j++) {
            const char *candidate = variants != NULL ? variants[j] : NULL;
            if (candidate != NULL && strcmp(candidate, variant_name) == 0) {
                int written = snprintf(out, out_size, "%s_%s",
                    ast_enum_name(stmt), candidate);
                if (written < 0 || (size_t)written >= out_size) {
                    out[0] = '\0';
                    return false;
                }
                return true;
            }
        }
    }

    return false;
}
