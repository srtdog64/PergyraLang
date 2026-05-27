#include "transpiler_event_emit.h"

#include <stdlib.h>

#include "transpiler_context.h"
#include "transpiler_type_render.h"
#include "../parser/ast_api.h"

void
emit_event_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = ast_event_name(node);
    const char *event_type = transpiler_scratch_fmt(ctx, "%s_Event", name);

    codebuf_write(ctx->out, "\n/* Event: %s */\n", name);
    codebuf_write(ctx->out, "typedef void (*%s_Handler)(", name);

    for (size_t i = 0; i < ast_event_param_count(node); i++) {
        ASTNode *param = ast_event_param(node, i);
        ASTNode *param_type = ast_let_type(param);
        char pt_buf[256];
        const char *pt = "void*";
        if (param_type != NULL
            && pergyra_ast_type_to_c_copy_in_ctx(ctx, param_type,
                pt_buf,
                sizeof(pt_buf))) {
            pt = pt_buf;
        }
        if (i > 0)
            codebuf_write(ctx->out, ", ");
        codebuf_write(ctx->out, "%s %s", pt, ast_let_name(param));
    }

    codebuf_write(ctx->out, ");\n");
    codebuf_write(ctx->out, "typedef struct {\n");
    codebuf_write(ctx->out, "    %s_Handler handlers[PGY_EVENT_MAX_HANDLERS];\n", name);
    codebuf_write(ctx->out, "    void* contexts[PGY_EVENT_MAX_HANDLERS];\n");
    codebuf_write(ctx->out, "    size_t count;\n");
    codebuf_write(ctx->out, "    bool is_invoking;\n");
    codebuf_write(ctx->out, "    bool pending_changes;\n");
    codebuf_write(ctx->out, "} %s;\n", event_type);
    codebuf_write(ctx->out, "static %s %s;\n", event_type, name);

    codebuf_write(ctx->out, "static inline void %s_INIT(%s* e) {\n", name, event_type);
    codebuf_write(ctx->out, "    memset(e, 0, sizeof(*e));\n");
    codebuf_write(ctx->out, "}\n");

    codebuf_write(ctx->out, "static inline void %s_SUBSCRIBE(%s* e, %s_Handler h) {\n", name, event_type, name);
    codebuf_write(ctx->out, "    if (e->count < PGY_EVENT_MAX_HANDLERS) {\n");
    codebuf_write(ctx->out, "        e->handlers[e->count++] = h;\n");
    codebuf_write(ctx->out, "    }\n");
    codebuf_write(ctx->out, "}\n");

    codebuf_write(ctx->out, "static inline void %s_UNSUBSCRIBE(%s* e, %s_Handler h) {\n", name, event_type, name);
    codebuf_write(ctx->out, "    for (size_t i = 0; i < e->count; i++) {\n");
    codebuf_write(ctx->out, "        if (e->handlers[i] == h) {\n");
    codebuf_write(ctx->out, "            for (size_t j = i; j < e->count - 1; j++) {\n");
    codebuf_write(ctx->out, "                e->handlers[j] = e->handlers[j + 1];\n");
    codebuf_write(ctx->out, "            }\n");
    codebuf_write(ctx->out, "            e->count--;\n");
    codebuf_write(ctx->out, "            break;\n");
    codebuf_write(ctx->out, "        }\n");
    codebuf_write(ctx->out, "    }\n");
    codebuf_write(ctx->out, "}\n");

    codebuf_write(ctx->out, "static inline void %s_INVOKE(%s* e", name, event_type);
    for (size_t i = 0; i < ast_event_param_count(node); i++) {
        ASTNode *param = ast_event_param(node, i);
        ASTNode *param_type = ast_let_type(param);
        char pt_buf[256];
        const char *pt = "void*";
        if (param_type != NULL
            && pergyra_ast_type_to_c_copy_in_ctx(ctx, param_type,
                pt_buf,
                sizeof(pt_buf))) {
            pt = pt_buf;
        }
        codebuf_write(ctx->out, ", %s %s", pt, ast_let_name(param));
    }
    codebuf_write(ctx->out, ") {\n");
    codebuf_write(ctx->out, "    e->is_invoking = true;\n");
    codebuf_write(ctx->out, "    for (size_t i = 0; i < e->count; i++) {\n");
    codebuf_write(ctx->out, "        e->handlers[i](");
    for (size_t i = 0; i < ast_event_param_count(node); i++) {
        ASTNode *param = ast_event_param(node, i);
        if (i > 0)
            codebuf_write(ctx->out, ", ");
        codebuf_write(ctx->out, "%s", ast_let_name(param));
    }
    codebuf_write(ctx->out, ");\n");
    codebuf_write(ctx->out, "    }\n");
    codebuf_write(ctx->out, "    e->is_invoking = false;\n");
    codebuf_write(ctx->out, "}\n");
}

void
emit_event_subscribe(ASTNode *node, TranspilerCtx *ctx)
{
    char *event_expr = emit_expression(ast_event_op_event(node), ctx);
    char *handler_expr = emit_expression(ast_event_op_handler(node), ctx);

    write_indent(ctx);
    codebuf_write(ctx->out, "%s_SUBSCRIBE(&%s, %s);\n",
                  event_expr, event_expr, handler_expr);

    free(event_expr);
    free(handler_expr);
}

void
emit_event_unsubscribe(ASTNode *node, TranspilerCtx *ctx)
{
    char *event_expr = emit_expression(ast_event_op_event(node), ctx);
    char *handler_expr = emit_expression(ast_event_op_handler(node), ctx);

    write_indent(ctx);
    codebuf_write(ctx->out, "%s_UNSUBSCRIBE(&%s, %s);\n",
                  event_expr, event_expr, handler_expr);

    free(event_expr);
    free(handler_expr);
}
