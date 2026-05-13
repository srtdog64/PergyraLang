#ifndef PGY_TRANSPILER_ZONE_METHODS_EMIT_H
#define PGY_TRANSPILER_ZONE_METHODS_EMIT_H

static void
transpiler_emit_zone_hosted_methods(const char *name,
                                    const TranspilerHostedMethodView *method_view,
                                    TranspilerCtx *ctx)
{
    for (size_t i = 0; method_view != NULL && i < method_view->count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(method_view, i);
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(method_view, i);
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;
        emit_hosted_method_forward_decl_from_metadata(name, method_meta,
            method, true, ctx->out, ctx);
    }

    emit_hosted_methods_from_mir_or_error_local(name, "(anonymous-zone)",
        "zone", method_view, ctx);
}

#endif /* PGY_TRANSPILER_ZONE_METHODS_EMIT_H */
