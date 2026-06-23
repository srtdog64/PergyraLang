/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend type-alias declaration emitter.
 */

#include "transpiler_type_alias.h"

#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_inventory_view.h"
#include "transpiler_specialization_registry.h"
#include "transpiler_type_require.h"

void
emit_type_alias_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *alias_name;
    const char *target_type_name;
    char target_c_type[256];

    if (node == NULL || ctx == NULL || node->type != AST_TYPE_ALIAS
        || ast_type_alias_name(node) == NULL) {
        return;
    }

    alias_name = ast_type_alias_name(node);
    if (transpiler_active_has_mir(ctx)) {
        target_type_name =
            transpiler_type_alias_target_type_name_from_headers(
                ctx, alias_name);
        if (target_type_name == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend: MIR type-alias target metadata missing for '%s'",
                alias_name);
            return;
        }
        ensure_type_specializations_from_type_name_to(
            ctx, ctx->decls, target_type_name);
        if (!transpiler_require_type_name_c_type_copy(
                ctx, target_type_name, "type alias target",
                target_c_type, sizeof(target_c_type))) {
            return;
        }
        codebuf_write(ctx->out, "typedef %s %s;\n", target_c_type, alias_name);
        return;
    }

    ensure_type_specializations_from_ast(ctx, ast_type_alias_target_type(node));
    if (!transpiler_require_ast_c_type_copy(
            ctx, ast_type_alias_target_type(node), "type alias target",
            target_c_type, sizeof(target_c_type))) {
        return;
    }
    codebuf_write(ctx->out, "typedef %s %s;\n", target_c_type, alias_name);
}
