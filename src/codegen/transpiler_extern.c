/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend extern declaration emission.
 */

#include <stdio.h>

#include "transpiler_extern.h"
#include "transpiler_type_require.h"
#include "transpiler_type_render.h"

void
emit_extern_block(ASTNode *node, TranspilerCtx *ctx)
{
    codebuf_write(ctx->out, "\n/* extern \"%s\" */\n",
                  node->data.extern_block.abi != NULL
                    ? node->data.extern_block.abi : "");

    for (size_t i = 0; i < node->data.extern_block.count; i++) {
        ASTNode *decl = node->data.extern_block.declarations[i];
        if (decl == NULL || decl->type != AST_FUNC_DECL)
            continue;

        const char *name = decl->data.func_decl.name;
        if (name == NULL)
            continue;
        const char *ret_type = "void";
        if (decl->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(decl->data.func_decl.return_type);

        codebuf_write(ctx->out, "%s %s(", ret_type, name);

        for (size_t j = 0; j < decl->data.func_decl.param_count; j++) {
            FuncParam *p = decl->data.func_decl.params[j];
            const char *pt = NULL;
            char surface_desc[256];
            snprintf(surface_desc, sizeof(surface_desc),
                "extern parameter '%s' of '%s'",
                p != NULL && p->name != NULL ? p->name : "(anonymous)",
                name != NULL ? name : "(anonymous)");
            pt = transpiler_require_ast_c_type(ctx,
                p != NULL ? p->type : NULL,
                surface_desc);
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
