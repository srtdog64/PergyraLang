#include "transpiler_event_builtin_emit.h"

#include <stdlib.h>
#include <stdio.h>

#include "transpiler_decl_lookup.h"

static char *
transpiler_event_strdup_invoke(const char *name, const char *args)
{
    const char *separator = (args != NULL && args[0] != '\0') ? ", " : "";
    const char *safe_args = (args != NULL) ? args : "";
    int needed = snprintf(NULL, 0, "%s_INVOKE(&%s%s%s)",
                          name, name, separator, safe_args);
    if (needed < 0)
        return NULL;
    if ((size_t)needed == (size_t)-1)
        return NULL;

    char *result = malloc((size_t)needed + 1);
    if (result == NULL)
        return NULL;

    int written = snprintf(result, (size_t)needed + 1,
        "%s_INVOKE(&%s%s%s)", name, name, separator, safe_args);
    if (written < 0 || written != needed) {
        free(result);
        return NULL;
    }
    return result;
}

char *
emit_call_event_builtin(ASTNode *call, ASTNode *callee, TranspilerCtx *ctx)
{
    if (call == NULL || callee == NULL || ctx == NULL)
        return NULL;

    if (callee->type == AST_IDENTIFIER) {
        const char *name = ast_identifier_name(callee);
        if (find_event_decl(ctx, name) != NULL) {
            CodeBuf *args_buf = codebuf_create();
            if (args_buf == NULL)
                return NULL;
            for (size_t i = 0; i < ast_call_arg_count(call); i++) {
                char *arg = emit_expression(ast_call_argument(call, i), ctx);
                if (i > 0)
                    codebuf_write(args_buf, ", ");
                codebuf_write(args_buf, "%s", arg);
                free(arg);
            }

            char *result = transpiler_event_strdup_invoke(name, args_buf->data);
            codebuf_destroy(args_buf);
            return result;
        }
    }

    return NULL;
}
