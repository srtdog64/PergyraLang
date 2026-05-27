/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend type-alias declaration emitter.
 */

#include "transpiler_type_alias.h"

#include "transpiler_type_render.h"

void
emit_type_alias_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *alias_name;
    char target_c_type[256];

    if (node == NULL || ctx == NULL || node->type != AST_TYPE_ALIAS
        || ast_type_alias_name(node) == NULL
        || ast_type_alias_target_type(node) == NULL) {
        return;
    }

    alias_name = ast_type_alias_name(node);
    ensure_type_specializations_from_ast(ctx, ast_type_alias_target_type(node));
    if (!pergyra_ast_type_to_c_copy_in_ctx(ctx, ast_type_alias_target_type(node),
            target_c_type,
            sizeof(target_c_type))) {
        target_c_type[0] = 'U';
        target_c_type[1] = 'n';
        target_c_type[2] = 'k';
        target_c_type[3] = 'n';
        target_c_type[4] = 'o';
        target_c_type[5] = 'w';
        target_c_type[6] = 'n';
        target_c_type[7] = '\0';
    }
    codebuf_write(ctx->out, "typedef %s %s;\n", target_c_type, alias_name);
}
