#ifndef PGY_SRC_CODEGEN_TRANSPILER_EVENT_BUILTIN_EMIT_H
#define PGY_SRC_CODEGEN_TRANSPILER_EVENT_BUILTIN_EMIT_H

static char *
emit_call_event_builtin(ASTNode *call, ASTNode *callee, TranspilerCtx *ctx)
{
    if (callee->type == AST_IDENTIFIER) {
        const char *name = callee->data.identifier.name;
        if (find_event_decl(ctx, name) != NULL) {
            CodeBuf *args_buf = codebuf_create();
            for (size_t i = 0; i < call->data.call.arg_count; i++) {
                char *arg = emit_expression(call->data.call.arguments[i], ctx);
                if (i > 0)
                    codebuf_write(args_buf, ", ");
                codebuf_write(args_buf, "%s", arg);
                free(arg);
            }

            char *result = strdup_fmt("%s_INVOKE(&%s%s%s)",
                                      name, name,
                                      args_buf->len > 0 ? ", " : "",
                                      args_buf->data);
            codebuf_destroy(args_buf);
            return result;
        }
    }

    return NULL;
}
#endif /* PGY_SRC_CODEGEN_TRANSPILER_EVENT_BUILTIN_EMIT_H */
