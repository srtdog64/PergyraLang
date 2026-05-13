#ifndef PGY_TRANSPILER_LET_TYPE_REGISTER_EMIT_H
#define PGY_TRANSPILER_LET_TYPE_REGISTER_EMIT_H

static void
transpiler_register_let_type_after_emit(TranspilerCtx *ctx,
                                        const char *name,
                                        ASTNode *init,
                                        char *ann_type_name)
{
    if (ann_type_name != NULL) {
        register_typed_var(ctx, name, ann_type_name);
        free(ann_type_name);
        return;
    }

    if (init != NULL && init->type == AST_CALL) {
        register_typed_var(ctx, name, infer_expression_type_name(ctx, init));
    } else if (init != NULL && init->type == AST_SPAWN_EXPR) {
        char *future_type = infer_spawn_return_type_name(ctx, init);
        char *wrapped = strdup_fmt("Future<%s>", future_type);
        register_typed_var(ctx, name, wrapped);
        free(future_type);
        free(wrapped);
    } else if (init != NULL && init->type == AST_CHANNEL_RECV) {
        const char *inner = infer_expression_type_name(ctx, init);
        register_typed_var(ctx, name, inner);
    } else if (init != NULL) {
        const char *inferred = infer_expression_type_name(ctx, init);
        if (inferred != NULL)
            register_typed_var(ctx, name, inferred);
    }
}

#endif /* PGY_TRANSPILER_LET_TYPE_REGISTER_EMIT_H */
