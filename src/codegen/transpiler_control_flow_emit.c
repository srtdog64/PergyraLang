#include "transpiler_control_flow_emit.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_symbols.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_require.h"

static bool
transpiler_loop_label_name(char *out, size_t out_size,
                           const char *kind, int loop_id)
{
    int written;

    if (out == NULL || out_size == 0 || kind == NULL)
        return false;
    written = snprintf(out, out_size, "_pgy_loop_%s_%d", kind, loop_id);
    return written >= 0 && (size_t)written < out_size;
}

static bool
transpiler_condition_is_already_parenthesized(const char *expr)
{
    size_t len;
    int depth = 0;
    bool in_string = false;
    bool escaped = false;

    if (expr == NULL)
        return false;
    len = strlen(expr);
    if (len < 2 || expr[0] != '(' || expr[len - 1] != ')')
        return false;

    for (size_t i = 0; i < len; i++) {
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (expr[i] == '\\') {
                escaped = true;
            } else if (expr[i] == '"') {
                in_string = false;
            }
            continue;
        }

        if (expr[i] == '"') {
            in_string = true;
        } else if (expr[i] == '(') {
            depth++;
        } else if (expr[i] == ')') {
            depth--;
            if (depth == 0 && i != len - 1)
                return false;
            if (depth < 0)
                return false;
        }
    }

    return depth == 0;
}

void
transpiler_write_condition_head(TranspilerCtx *ctx,
                                const char *keyword,
                                const char *expr,
                                const char *suffix)
{
    const char *safe_expr = expr != NULL ? expr : "false";
    const char *safe_suffix = suffix != NULL ? suffix : "";

    if (transpiler_condition_is_already_parenthesized(safe_expr)) {
        codebuf_write(ctx->out, "%s %s%s", keyword, safe_expr, safe_suffix);
        return;
    }

    codebuf_write(ctx->out, "%s (%s)%s", keyword, safe_expr, safe_suffix);
}

void
emit_if_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    char *cond = emit_expression(ast_if_condition(node), ctx);
    write_indent(ctx);
    transpiler_write_condition_head(ctx, "if", cond, "\n");
    free(cond);

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    if (ast_if_then_branch(node) != NULL)
        emit_block(ast_if_then_branch(node), ctx);
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");

    if (ast_if_else_branch(node) != NULL) {
        write_indent(ctx);
        codebuf_write(ctx->out, "else\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;
        emit_statement(ast_if_else_branch(node), ctx);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
}

int
transpiler_find_loop_label_depth(const TranspilerCtx *ctx, const char *label)
{
    if (ctx == NULL || label == NULL)
        return -1;

    for (int i = ctx->loop_depth - 1; i >= 0; i--) {
        if (ctx->loop_labels[i] != NULL
            && strcmp(ctx->loop_labels[i], label) == 0) {
            return i;
        }
    }

    return -1;
}

void
emit_for_loop(ASTNode *node, TranspilerCtx *ctx)
{
    const char *var = ast_for_variable(node);
    ASTNode *iterable = ast_for_iterable(node);
    ASTNode *body = ast_for_body(node);
    int loop_slot = ctx->loop_depth;
    int loop_id = ++ctx->tmp_counter;

    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH) {
        ctx->loop_labels[loop_slot] = ast_for_label(node);
        if (!transpiler_loop_label_name(ctx->loop_break_labels[loop_slot],
                sizeof(ctx->loop_break_labels[loop_slot]), "break", loop_id)
            || !transpiler_loop_label_name(ctx->loop_continue_labels[loop_slot],
                sizeof(ctx->loop_continue_labels[loop_slot]), "continue",
                loop_id)) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend: generated for-loop cleanup label is too long");
            return;
        }
        ctx->loop_break_label_used[loop_slot] = false;
        ctx->loop_continue_label_used[loop_slot] = false;
        ctx->loop_defer_base_depth[loop_slot] = ctx->defer_scope_depth;
        ctx->loop_depth++;
    }

    if (iterable != NULL) {
        char *coll = emit_expression(iterable, ctx);
        const char *coll_type = infer_expression_type_name(ctx, iterable);
        char elem_inner_buf[128];
        char elem_type_buf[128];
        const char *elem_inner = NULL;
        const char *elem_type = NULL;
        const char *length_field = "count";
        if (transpiler_type_name_is_array_or_slice(coll_type)) {
            if (slot_inner_type_name_copy(coll_type, elem_inner_buf,
                    sizeof(elem_inner_buf)))
                elem_inner = elem_inner_buf;
            if (transpiler_require_type_name_c_type_copy(ctx, elem_inner,
                    "for-in iterable element", elem_type_buf,
                    sizeof(elem_type_buf))) {
                elem_type = elem_type_buf;
            }
            length_field = "length";
        } else if (transpiler_type_name_is_list(coll_type)) {
            if (slot_inner_type_name_copy(coll_type, elem_inner_buf,
                    sizeof(elem_inner_buf)))
                elem_inner = elem_inner_buf;
            if (transpiler_require_type_name_c_type_copy(ctx, elem_inner,
                    "for-in iterable element", elem_type_buf,
                    sizeof(elem_type_buf))) {
                elem_type = elem_type_buf;
            }
            length_field = "count";
        }
        if (elem_type == NULL) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "cannot derive concrete element type for for-in iterable '%s'",
                coll_type != NULL ? coll_type : "(unknown)");
            free(coll);
            return;
        }

        int idx_id = ++ctx->tmp_counter;
        write_indent(ctx);
        codebuf_write(ctx->out,
            "for (size_t _pgy_idx_%d = 0; "
            "_pgy_idx_%d < %s.%s; "
            "_pgy_idx_%d++)\n",
            idx_id, idx_id, coll, length_field, idx_id);
        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s = %s.data[_pgy_idx_%d];\n",
            elem_type, var, coll, idx_id);
        register_typed_var(ctx, var, elem_inner);
        if (body != NULL)
            emit_block(body, ctx);
        if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
            && ctx->loop_continue_label_used[loop_slot]) {
            write_indent(ctx);
            codebuf_write(ctx->out, "%s: ;\n",
                ctx->loop_continue_labels[loop_slot]);
        }
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
        if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
            && ctx->loop_break_label_used[loop_slot]) {
            write_indent(ctx);
            codebuf_write(ctx->out, "%s: ;\n",
                ctx->loop_break_labels[loop_slot]);
        }
        if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH) {
            ctx->loop_depth--;
            ctx->loop_labels[loop_slot] = NULL;
        }
        free(coll);
        return;
    }

    char *start = emit_expression(ast_for_range_start(node), ctx);
    char *end   = emit_expression(ast_for_range_end(node), ctx);

    write_indent(ctx);
    codebuf_write(ctx->out,
        "for (int32_t %s = %s; %s < %s; %s++)\n",
        var, start, var, end, var);
    free(start);
    free(end);

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    if (body != NULL)
        emit_block(body, ctx);
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
        && ctx->loop_continue_label_used[loop_slot]) {
        write_indent(ctx);
        codebuf_write(ctx->out, "%s: ;\n",
            ctx->loop_continue_labels[loop_slot]);
    }
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
        && ctx->loop_break_label_used[loop_slot]) {
        write_indent(ctx);
        codebuf_write(ctx->out, "%s: ;\n",
            ctx->loop_break_labels[loop_slot]);
    }
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH) {
        ctx->loop_depth--;
        ctx->loop_labels[loop_slot] = NULL;
    }
}

void
emit_while_loop(ASTNode *node, TranspilerCtx *ctx)
{
    char *cond = emit_expression(ast_while_condition(node), ctx);
    int loop_slot = ctx->loop_depth;
    int loop_id = ++ctx->tmp_counter;
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH) {
        ctx->loop_labels[loop_slot] = ast_while_label(node);
        if (!transpiler_loop_label_name(ctx->loop_break_labels[loop_slot],
                sizeof(ctx->loop_break_labels[loop_slot]), "break", loop_id)
            || !transpiler_loop_label_name(ctx->loop_continue_labels[loop_slot],
                sizeof(ctx->loop_continue_labels[loop_slot]), "continue",
                loop_id)) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend: generated while-loop cleanup label is too long");
            free(cond);
            return;
        }
        ctx->loop_break_label_used[loop_slot] = false;
        ctx->loop_continue_label_used[loop_slot] = false;
        ctx->loop_defer_base_depth[loop_slot] = ctx->defer_scope_depth;
        ctx->loop_depth++;
    }
    write_indent(ctx);
    transpiler_write_condition_head(ctx, "while", cond, "\n");
    free(cond);

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    if (ast_while_body(node) != NULL)
        emit_block(ast_while_body(node), ctx);
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
        && ctx->loop_continue_label_used[loop_slot]) {
        write_indent(ctx);
        codebuf_write(ctx->out, "%s: ;\n",
            ctx->loop_continue_labels[loop_slot]);
    }
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
        && ctx->loop_break_label_used[loop_slot]) {
        write_indent(ctx);
        codebuf_write(ctx->out, "%s: ;\n",
            ctx->loop_break_labels[loop_slot]);
    }
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH) {
        ctx->loop_depth--;
        ctx->loop_labels[loop_slot] = NULL;
    }
}
