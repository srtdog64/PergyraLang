/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend type-alias declaration emitter.
 */

#include "transpiler_type_alias.h"

#include "../compiler/mir_decl_headers.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_inventory_view.h"
#include "transpiler_specialization_registry.h"
#include "transpiler_type_render.h"
#include "transpiler_type_mapping.h"

#include <string.h>

static const char *
transpiler_type_alias_target_type_name_from_headers(TranspilerCtx *ctx,
                                                    const char *alias_name)
{
    const char *current = alias_name;

    if (ctx == NULL || alias_name == NULL)
        return NULL;

    for (size_t depth = 0; depth < 32; depth++) {
        const MIRDeclHeader *alias_header =
            transpiler_active_decl_header_of_type(
                ctx, AST_TYPE_ALIAS, current);
        const char *target_type_name =
            mir_decl_header_type_alias_target_type_name(alias_header);

        if (target_type_name == NULL)
            return depth == 0 ? NULL : current;
        current = target_type_name;
        if (strchr(current, '<') != NULL || strchr(current, '(') != NULL)
            return current;
    }

    return current;
}

void
emit_type_alias_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *alias_name;
    const char *target_type_name;
    char target_c_type[256];

    if (node == NULL || ctx == NULL || node->type != AST_TYPE_ALIAS
        || ast_type_alias_name(node) == NULL
        || (!transpiler_active_has_mir(ctx)
            && ast_type_alias_target_type(node) == NULL)) {
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
            ctx, ctx->out, target_type_name);
        if (!pergyra_type_to_c_copy(
                target_type_name, target_c_type, sizeof(target_c_type))) {
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
        return;
    }

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
