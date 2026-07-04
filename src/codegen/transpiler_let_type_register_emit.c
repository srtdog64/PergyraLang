/*
 * Copyright (c) 2026 Pergyra Language Project
 * Post-emission let type registration for C backend lowering.
 */

#include "transpiler_let_type_register_emit.h"

#include <stdlib.h>

#include "../parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_future_type_query.h"
#include "transpiler_symbols.h"
#include "codegen_type_mapping.h"

static void
register_let_type_fact_if_concrete(TranspilerCtx *ctx,
                                   const char *name,
                                   const char *type_name)
{
    if (transpiler_type_name_is_concrete_fact(type_name))
        register_typed_var(ctx, name, type_name);
}

void
transpiler_register_let_type_after_emit(TranspilerCtx *ctx,
                                        const char *name,
                                        ASTNode *init,
                                        char *ann_type_name)
{
    if (ann_type_name != NULL) {
        register_let_type_fact_if_concrete(ctx, name, ann_type_name);
        free(ann_type_name);
        return;
    }

    if (init != NULL && init->type == AST_CALL) {
        register_let_type_fact_if_concrete(ctx, name,
            infer_expression_type_name(ctx, init));
    } else if (init != NULL && init->type == AST_SPAWN_EXPR) {
        const char *future_type = infer_spawn_return_type_name_scratch(
            ctx, init);
        if (!transpiler_type_name_is_concrete_fact(future_type))
            return;
        const char *wrapped = transpiler_scratch_fmt(ctx, "Future<%s>",
                                                     future_type);
        register_let_type_fact_if_concrete(ctx, name, wrapped);
    } else if (init != NULL && init->type == AST_CHANNEL_RECV) {
        const char *inner = infer_expression_type_name(ctx, init);
        register_let_type_fact_if_concrete(ctx, name, inner);
    } else if (init != NULL) {
        const char *inferred = infer_expression_type_name(ctx, init);
        register_let_type_fact_if_concrete(ctx, name, inferred);
    }
}
