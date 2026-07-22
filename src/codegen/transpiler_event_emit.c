#include "transpiler_event_emit.h"

#include <stdlib.h>

#include "../compiler/mir_decl_headers.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_type_render.h"
#include "transpiler_type_require.h"
#include "../parser/ast_api.h"

static char *
transpiler_event_op_emit_part(TranspilerCtx *ctx,
                              ASTNode *expr,
                              const char *op_name,
                              const char *role)
{
    char *lowered = emit_expression(expr, ctx);
    if (lowered != NULL)
        return lowered;

    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "C backend: event %s could not lower %s expression",
        op_name != NULL ? op_name : "operation",
        role != NULL ? role : "operand");
    return NULL;
}

static void
emit_event_decl_from_mir_header(const MIRDeclHeader *header,
                                TranspilerCtx *ctx)
{
    const char *name;
    const char *event_type;
    size_t param_count;

    if (ctx == NULL || header == NULL) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing event declaration header metadata");
        return;
    }
    name = mir_decl_header_name(header);
    if (name == NULL || name[0] == '\0'
        || mir_decl_header_ast_type_or(header, AST_PROGRAM)
            != AST_EVENT_DECL) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing event declaration identity metadata");
        return;
    }
    param_count = mir_decl_header_event_param_count(header);
    event_type = transpiler_scratch_fmt(ctx, "%s_Event", name);

    codebuf_write(ctx->out, "\n/* Event: %s */\n", name);
    codebuf_write(ctx->out, "typedef void (*%s_Handler)(", name);
    for (size_t i = 0; i < param_count; i++) {
        const char *param_name =
            mir_decl_header_event_param_name(header, i);
        const char *param_type_name =
            mir_decl_header_event_param_type_name(header, i);
        char pt_buf[256];

        if (param_name == NULL || param_type_name == NULL) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing event parameter ABI metadata for '%s' index %zu",
                name, i);
            return;
        }
        if (!transpiler_require_type_name_c_type_copy(ctx, param_type_name,
                "event handler parameter", pt_buf, sizeof(pt_buf)))
            return;
        if (i > 0)
            codebuf_write(ctx->out, ", ");
        codebuf_write(ctx->out, "%s %s", pt_buf, param_name);
    }
    codebuf_write(ctx->out, ");\n");
    codebuf_write(ctx->out, "typedef struct {\n");
    codebuf_write(ctx->out,
        "    %s_Handler handlers[PGY_EVENT_MAX_HANDLERS];\n", name);
    codebuf_write(ctx->out,
        "    void* contexts[PGY_EVENT_MAX_HANDLERS];\n");
    codebuf_write(ctx->out, "    size_t count;\n");
    codebuf_write(ctx->out, "    bool is_invoking;\n");
    codebuf_write(ctx->out, "    bool pending_changes;\n");
    codebuf_write(ctx->out, "} %s;\n", event_type);
    codebuf_write(ctx->out, "static %s %s;\n", event_type, name);

    codebuf_write(ctx->out,
        "static inline void %s_INIT(%s* e) {\n", name, event_type);
    codebuf_write(ctx->out, "    memset(e, 0, sizeof(*e));\n}\n");
    codebuf_write(ctx->out,
        "static inline void %s_SUBSCRIBE(%s* e, %s_Handler h) {\n",
        name, event_type, name);
    codebuf_write(ctx->out,
        "    if (e->count < PGY_EVENT_MAX_HANDLERS) {\n");
    codebuf_write(ctx->out,
        "        e->handlers[e->count++] = h;\n        return;\n    }\n");
    codebuf_write(ctx->out,
        "    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \"event handler capacity exceeded\");\n}\n");
    codebuf_write(ctx->out,
        "static inline void %s_UNSUBSCRIBE(%s* e, %s_Handler h) {\n",
        name, event_type, name);
    codebuf_write(ctx->out,
        "    for (size_t i = 0; i < e->count; i++) {\n"
        "        if (e->handlers[i] == h) {\n"
        "            for (size_t j = i; j < e->count - 1; j++)\n"
        "                e->handlers[j] = e->handlers[j + 1];\n"
        "            e->count--;\n            break;\n        }\n    }\n}\n");

    codebuf_write(ctx->out,
        "static inline void %s_INVOKE(%s* e", name, event_type);
    for (size_t i = 0; i < param_count; i++) {
        const char *param_name =
            mir_decl_header_event_param_name(header, i);
        const char *param_type_name =
            mir_decl_header_event_param_type_name(header, i);
        char pt_buf[256];

        if (param_name == NULL || param_type_name == NULL) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing event invoke ABI metadata for '%s' index %zu",
                name, i);
            return;
        }
        if (!transpiler_require_type_name_c_type_copy(ctx, param_type_name,
                "event invoke parameter", pt_buf, sizeof(pt_buf)))
            return;
        codebuf_write(ctx->out, ", %s %s", pt_buf, param_name);
    }
    codebuf_write(ctx->out, ") {\n    e->is_invoking = true;\n");
    codebuf_write(ctx->out,
        "    for (size_t i = 0; i < e->count; i++) {\n        e->handlers[i](");
    for (size_t i = 0; i < param_count; i++) {
        if (i > 0)
            codebuf_write(ctx->out, ", ");
        codebuf_write(ctx->out, "%s",
            mir_decl_header_event_param_name(header, i));
    }
    codebuf_write(ctx->out,
        ");\n    }\n    e->is_invoking = false;\n}\n");
}

void
emit_event_decl(ASTNode *node, TranspilerCtx *ctx)
{
    if (transpiler_active_has_mir(ctx)) {
        const char *event_name = ast_event_name(node);
        const MIRDeclHeader *header =
            transpiler_active_decl_header_of_type(
                ctx, AST_EVENT_DECL, event_name);
        emit_event_decl_from_mir_header(header, ctx);
        return;
    }

    const char *name = ast_event_name(node);
    const char *event_type = transpiler_scratch_fmt(ctx, "%s_Event", name);

    codebuf_write(ctx->out, "\n/* Event: %s */\n", name);
    codebuf_write(ctx->out, "typedef void (*%s_Handler)(", name);

    for (size_t i = 0; i < ast_event_param_count(node); i++) {
        ASTNode *param = ast_event_param(node, i);
        ASTNode *param_type = ast_let_type(param);
        char pt_buf[256];
        const char *pt = pt_buf;
        if (!transpiler_require_ast_c_type_copy(ctx,
                param_type,
                "event handler parameter",
                pt_buf,
                sizeof(pt_buf)))
            return;
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
    codebuf_write(ctx->out, "        return;\n");
    codebuf_write(ctx->out, "    }\n");
    codebuf_write(ctx->out, "    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \"event handler capacity exceeded\");\n");
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
        const char *pt = pt_buf;
        if (!transpiler_require_ast_c_type_copy(ctx,
                param_type,
                "event invoke parameter",
                pt_buf,
                sizeof(pt_buf)))
            return;
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
    char *event_expr = transpiler_event_op_emit_part(ctx,
        ast_event_op_event(node), "subscribe", "event");
    char *handler_expr = transpiler_event_op_emit_part(ctx,
        ast_event_op_handler(node), "subscribe", "handler");
    if (event_expr == NULL || handler_expr == NULL) {
        free(event_expr);
        free(handler_expr);
        return;
    }

    write_indent(ctx);
    codebuf_write(ctx->out, "%s_SUBSCRIBE(&%s, %s);\n",
                  event_expr, event_expr, handler_expr);

    free(event_expr);
    free(handler_expr);
}

void
emit_event_unsubscribe(ASTNode *node, TranspilerCtx *ctx)
{
    char *event_expr = transpiler_event_op_emit_part(ctx,
        ast_event_op_event(node), "unsubscribe", "event");
    char *handler_expr = transpiler_event_op_emit_part(ctx,
        ast_event_op_handler(node), "unsubscribe", "handler");
    if (event_expr == NULL || handler_expr == NULL) {
        free(event_expr);
        free(handler_expr);
        return;
    }

    write_indent(ctx);
    codebuf_write(ctx->out, "%s_UNSUBSCRIBE(&%s, %s);\n",
                  event_expr, event_expr, handler_expr);

    free(event_expr);
    free(handler_expr);
}
