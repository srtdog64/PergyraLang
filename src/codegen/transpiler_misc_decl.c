/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend small declaration emitters.
 */

#include "transpiler.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "../semantic/diag_codes.h"

void
emit_include_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    const char *included_role = ast_include_role_name(node);

    codebuf_write(ctx->out, "/* include %s */\n", included_role);
    if (find_role_decl(ctx, included_role) == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "cannot resolve included role '%s' while emitting include statement",
            included_role != NULL ? included_role : "<role>");
    }
}

void
emit_impl_ability(ASTNode *node, TranspilerCtx *ctx)
{
    const char *ability_name = ast_impl_ability_name(node);

    codebuf_write(ctx->out, "/* Impl ability: %s */\n", ability_name);
    /* This is handled within emit_role_decl. */
    (void)ctx;
}
