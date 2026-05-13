/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend enum lowering helpers.
 */

#include <stdio.h>
#include <string.h>

#include "transpiler_enum.h"

const char *
lookup_enum_variant_qualified_name(TranspilerCtx *ctx, const char *variant_name)
{
    static char qualified[128];
    ASTNode **types = NULL;
    size_t type_count = 0;

    if (ctx == NULL || variant_name == NULL)
        return NULL;

    transpiler_active_inventory(ctx, AST_ENUM_DECL, &types, &type_count);
    if (types == NULL)
        return NULL;

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
                snprintf(qualified, sizeof(qualified), "%s_%s",
                    ast_enum_name(stmt), candidate);
                return qualified;
            }
        }
    }

    return NULL;
}
