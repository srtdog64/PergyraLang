/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR role lookup helpers.
 */

#include "transpiler_mir_role_lookup.h"

#include <string.h>

#include "transpiler_decl_lookup.h"

const MIRRoutine *
transpiler_find_role_impl_mir_method(const TranspilerCtx *ctx,
                                     const char *owner_name,
                                     const ASTNode *method_decl)
{
    const char *target = NULL;
    const MIRDeclHeader *header = NULL;

    if (ctx == NULL || ctx->mir == NULL || owner_name == NULL
        || method_decl == NULL || method_decl->type != AST_FUNC_DECL
        || method_decl->data.func_decl.name == NULL) {
        return NULL;
    }

    target = method_decl->data.func_decl.name;
    header = mir_find_decl_header(ctx->mir, owner_name);
    if (header == NULL || header->ast_type != AST_ROLE_DECL)
        return NULL;

    for (size_t i = 0; i < header->method_metadata_count; i++) {
        const MIRDeclMethod *method = &header->method_metadata[i];
        if (method->name == NULL || strcmp(method->name, target) != 0)
            continue;
        return transpiler_mir_decl_method_routine(ctx, method);
    }

    return NULL;
}
