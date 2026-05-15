/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend extern declaration emission.
 */

#include <stdio.h>

#include "../parser/ast_api.h"

#include "transpiler_extern.h"
#include "transpiler_type_require.h"
#include "transpiler_type_render.h"

void
emit_extern_block(ASTNode *node, TranspilerCtx *ctx)
{
    codebuf_write(ctx->out, "\n/* extern \"%s\" */\n",
                  ast_extern_block_abi(node) != NULL
                    ? ast_extern_block_abi(node) : "");

    size_t extern_count = 0;
    (void)ast_extern_block_declarations(node, &extern_count);
    for (size_t i = 0; i < extern_count; i++) {
        ASTNode *decl = ast_extern_block_declaration(node, i);
        if (decl == NULL || decl->type != AST_FUNC_DECL)
            continue;

        const char *name = ast_declaration_name(decl);
        if (name == NULL)
            continue;
        const char *ret_type = "void";
        char ret_type_buf[256];
        if (ast_func_return_type(decl) != NULL
            && transpiler_require_ast_c_type_copy(ctx,
                ast_func_return_type(decl),
                "extern return type",
                ret_type_buf,
                sizeof(ret_type_buf))) {
            ret_type = ret_type_buf;
        }

        codebuf_write(ctx->out, "%s %s(", ret_type, name);

        for (size_t j = 0; j < ast_func_param_count(decl); j++) {
            FuncParam *p = ast_func_param(decl, j);
            const char *pt = NULL;
            char pt_buf[256];
            char surface_desc[256];
            snprintf(surface_desc, sizeof(surface_desc),
                "extern parameter '%s' of '%s'",
                p != NULL && p->name != NULL ? p->name : "(anonymous)",
                name != NULL ? name : "(anonymous)");
            if (transpiler_require_ast_c_type_copy(ctx,
                    p != NULL ? p->type : NULL,
                    surface_desc,
                    pt_buf,
                    sizeof(pt_buf))) {
                pt = pt_buf;
            }
            if (pt == NULL)
                return;
            if (p == NULL || p->name == NULL)
                return;
            if (j > 0)
                codebuf_write(ctx->out, ", ");
            codebuf_write(ctx->out, "%s %s", pt, p->name);
        }

        codebuf_write(ctx->out, ");\n");
    }
}
