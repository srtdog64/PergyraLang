#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "transpiler.h"
#include "transpiler_context.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_format.h"
#include "transpiler_mir_emit_state.h"
#include "transpiler_symbols.h"
#include "transpiler_type_render.h"
#include "transpiler_type_require.h"

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"

static ASTNode *
transpiler_lambda_expected_return_type(TranspilerCtx *ctx, ASTNode *lambda)
{
    ASTNode *expected;

    if (ctx == NULL || lambda == NULL)
        return NULL;
    expected = ctx->expected_callable_type;
    if (expected == NULL || expected->type != AST_EVENT_HANDLER_TYPE)
        return NULL;
    if (ast_event_handler_param_count(expected)
        != ast_lambda_param_count(lambda)) {
        return NULL;
    }
    return ast_event_handler_return_type(expected);
}

static ASTNode *
transpiler_lambda_expected_param_type_at(TranspilerCtx *ctx, ASTNode *lambda,
                                         size_t param_index)
{
    ASTNode *expected;
    size_t param_count;

    if (ctx == NULL || lambda == NULL)
        return NULL;
    expected = ctx->expected_callable_type;
    if (expected == NULL || expected->type != AST_EVENT_HANDLER_TYPE)
        return NULL;
    param_count = ast_lambda_param_count(lambda);
    if (ast_event_handler_param_count(expected) != param_count)
        return NULL;
    if (param_index >= param_count)
        return NULL;
    return ast_event_handler_param_type(expected, param_index);
}

static bool
transpiler_infer_lambda_param_c_type_copy(TranspilerCtx *ctx,
                                          ASTNode *lambda_node,
                                          ASTNode *param_node,
                                          char *out,
                                          size_t out_size)
{
    const char *param_name = NULL;
    ASTNode *body = NULL;
    ASTNode *ret_value = NULL;

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';
    if (lambda_node == NULL || param_node == NULL)
        return false;
    if (param_node->type == AST_IDENTIFIER)
        param_name = ast_identifier_name(param_node);
    else if (param_node->type == AST_LET_DECL)
        param_name = ast_let_name(param_node);
    if (param_name == NULL || ast_lambda_return_type(lambda_node) == NULL)
        return false;

    body = ast_lambda_body(lambda_node);
    if (body == NULL)
        return false;
    if (body->type == AST_IDENTIFIER) {
        ret_value = body;
    } else if (body->type == AST_BLOCK
               && ast_block_statement_count(body) == 1
               && ast_block_statement(body, 0) != NULL
               && ast_block_statement(body, 0)->type == AST_RETURN) {
        ret_value = ast_return_value(ast_block_statement(body, 0));
    }

    if (ret_value != NULL
        && ret_value->type == AST_IDENTIFIER
        && ast_identifier_name(ret_value) != NULL
        && strcmp(ast_identifier_name(ret_value), param_name) == 0) {
        return pergyra_ast_type_to_c_copy_in_ctx(ctx,
            ast_lambda_return_type(lambda_node),
            out,
            out_size);
    }
    return false;
}

static bool
transpiler_lambda_param_c_type_copy(TranspilerCtx *ctx, ASTNode *lambda_node,
                                    ASTNode *param, size_t param_index,
                                    char *out, size_t out_size,
                                    const char **param_name_out)
{
    const char *param_name = NULL;
    const char *param_type = NULL;

    if (param_name_out != NULL)
        *param_name_out = NULL;
    if (param == NULL || out == NULL || out_size == 0)
        return false;
    out[0] = '\0';

    if (param->type == AST_LET_DECL) {
        ASTNode *param_type_ast = ast_let_type(param);
        param_name = ast_let_name(param);
        if (param_type_ast != NULL
            && pergyra_ast_type_to_c_copy_in_ctx(ctx, param_type_ast,
                out,
                out_size)) {
            param_type = out;
        } else {
            ASTNode *expected_param_type =
                transpiler_lambda_expected_param_type_at(ctx, lambda_node,
                    param_index);
            if (expected_param_type != NULL
                && pergyra_ast_type_to_c_copy_in_ctx(ctx, expected_param_type,
                    out, out_size)) {
                param_type = out;
            }
        }
    } else {
        ASTNode *expected_param_type =
            transpiler_lambda_expected_param_type_at(ctx, lambda_node,
                param_index);
        param_name = ast_identifier_name(param);
        if (expected_param_type != NULL
            && pergyra_ast_type_to_c_copy_in_ctx(ctx, expected_param_type,
                out, out_size)) {
            param_type = out;
        } else if (transpiler_infer_lambda_param_c_type_copy(ctx, lambda_node,
                param,
                out,
                out_size)) {
            param_type = out;
        }
    }
    if (param_name_out != NULL)
        *param_name_out = param_name;
    return param_type != NULL;
}

static bool
transpiler_emit_lambda_signature(ASTNode *node, TranspilerCtx *ctx,
                                 CodeBuf *out, const char *return_type,
                                 const char *lambda_name,
                                 bool declaration_only)
{
    codebuf_write(out, "\nstatic %s %s(", return_type, lambda_name);
    for (size_t i = 0; i < ast_lambda_param_count(node); i++) {
        ASTNode *param = ast_lambda_param(node, i);
        const char *param_name = NULL;
        char param_type_buf[256];
        if (i > 0)
            codebuf_write(out, ", ");
        if (!transpiler_lambda_param_c_type_copy(ctx, node, param,
                i,
                param_type_buf, sizeof(param_type_buf), &param_name)) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "cannot determine lambda parameter type for '%s' at argument %llu",
                lambda_name,
                (unsigned long long) i);
            return false;
        }
        codebuf_write(out, "%s %s", param_type_buf, param_name);
    }
    codebuf_write(out, declaration_only ? ");\n" : ")\n{\n");
    return true;
}

char *
emit_lambda_expr(ASTNode *node, TranspilerCtx *ctx)
{
    int lambda_id = ++ctx->tmp_counter;
    ASTNode *lambda_body = ast_lambda_body(node);
    ASTNode *lambda_return_type = ast_lambda_return_type(node);
    const char *return_type = NULL;
    char inferred_return_c_type_buf[256];
    char *return_type_owned = NULL;
    int saved_slot_var_count = ctx->slot_var_count;
    int saved_typed_var_count = ctx->typed_var_count;
    int saved_alias_var_count = ctx->alias_var_count;

    for (size_t i = 0; i < ast_lambda_param_count(node); i++) {
        ASTNode *param = ast_lambda_param(node, i);
        const char *param_name = NULL;
        char *param_type_name = NULL;

        if (param == NULL)
            continue;
        if (param->type == AST_LET_DECL) {
            param_name = ast_let_name(param);
            if (ast_let_type(param) != NULL)
                param_type_name = render_type_name_in_ctx(
                    ctx, ast_let_type(param));
        } else if (param->type == AST_IDENTIFIER) {
            ASTNode *expected_param_type =
                transpiler_lambda_expected_param_type_at(ctx, node, i);
            param_name = ast_identifier_name(param);
            if (expected_param_type != NULL)
                param_type_name = render_type_name_in_ctx(
                    ctx, expected_param_type);
        }
        if (param_name != NULL && param_type_name != NULL)
            register_typed_var(ctx, param_name, param_type_name);
        free(param_type_name);
    }

    if (lambda_return_type != NULL) {
        if (pergyra_ast_type_to_c_copy_in_ctx(ctx, lambda_return_type,
                inferred_return_c_type_buf,
                sizeof(inferred_return_c_type_buf))) {
            return_type = inferred_return_c_type_buf;
        }
    } else {
        ASTNode *expected_return_type =
            transpiler_lambda_expected_return_type(ctx, node);
        if (expected_return_type != NULL
            && pergyra_ast_type_to_c_copy_in_ctx(ctx,
                expected_return_type,
                inferred_return_c_type_buf,
                sizeof(inferred_return_c_type_buf))) {
            return_type = inferred_return_c_type_buf;
        }
    }
    if (return_type == NULL && lambda_body != NULL
        && lambda_body->type == AST_BLOCK) {
        return_type = "void";
    } else if (return_type == NULL && lambda_body != NULL) {
        const char *inferred_return_type =
            transpiler_expr_infer_type_name(ctx, lambda_body);
        if (inferred_return_type != NULL
            && transpiler_require_type_name_c_type_copy(ctx,
                inferred_return_type,
                "lambda inferred return",
                inferred_return_c_type_buf,
                sizeof(inferred_return_c_type_buf))) {
            return_type = inferred_return_c_type_buf;
        }
    }
    if (return_type == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "cannot determine lambda return type; explicit return type is required for non-block lambda bodies");
        transpiler_restore_local_binding_counts_local(
            ctx, saved_slot_var_count, saved_typed_var_count,
            saved_alias_var_count);
        return NULL;
    }
    return_type_owned = pergyra_strdup(return_type);
    if (return_type_owned == NULL) {
        transpiler_restore_local_binding_counts_local(
            ctx, saved_slot_var_count, saved_typed_var_count,
            saved_alias_var_count);
        return NULL;
    }
    return_type = return_type_owned;

    char *lambda_name = strdup_fmt("pgy_lambda_%d", lambda_id);
    if (lambda_name == NULL) {
        free(return_type_owned);
        transpiler_restore_local_binding_counts_local(
            ctx, saved_slot_var_count, saved_typed_var_count,
            saved_alias_var_count);
        return NULL;
    }

    if (!transpiler_emit_lambda_signature(node, ctx, ctx->decls,
            return_type, lambda_name, true)
        || !transpiler_emit_lambda_signature(node, ctx, ctx->helpers,
            return_type, lambda_name, false)) {
        free(lambda_name);
        free(return_type_owned);
        transpiler_restore_local_binding_counts_local(
            ctx, saved_slot_var_count, saved_typed_var_count,
            saved_alias_var_count);
        return NULL;
    }

    if (lambda_body != NULL && lambda_body->type == AST_BLOCK) {
        CodeBuf *saved_out = ctx->out;
        int saved_indent = ctx->indent;
        ctx->out = ctx->helpers;
        ctx->indent = 1;
        emit_block(lambda_body, ctx);
        ctx->indent = saved_indent;
        ctx->out = saved_out;
    } else if (lambda_body != NULL) {
        char *expr = emit_expression(lambda_body, ctx);
        if (expr == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C lambda expression could not lower body expression");
            free(lambda_name);
            free(return_type_owned);
            transpiler_restore_local_binding_counts_local(
                ctx, saved_slot_var_count, saved_typed_var_count,
                saved_alias_var_count);
            return NULL;
        }
        write_indent_to(ctx->helpers, 1);
        codebuf_write(ctx->helpers, "return %s;\n", expr);
        free(expr);
    }

    codebuf_write(ctx->helpers, "}\n");
    free(return_type_owned);
    transpiler_restore_local_binding_counts_local(
        ctx, saved_slot_var_count, saved_typed_var_count,
        saved_alias_var_count);
    return lambda_name;
}
