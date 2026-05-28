/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend Log/LogRaw/LogBanner builtin emission.
 */

#include "transpiler.h"
#include "transpiler_context.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_format.h"
#include "transpiler_log_normalize.h"

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"

#include <stdlib.h>
#include <string.h>

static char *
emit_c_log_call_for_expr(ASTNode *arg_node, char *arg, TranspilerCtx *ctx)
{
    const char *type_name;

    if (arg == NULL)
        return pergyra_strdup("0");
    type_name = transpiler_expr_infer_type_name(ctx, arg_node);
    if (type_name != NULL && strcmp(type_name, "Bool") == 0)
        return strdup_fmt("pgy_log_bool((bool)(%s))", arg);
    return strdup_fmt("pgy_log(%s)", arg);
}

char *
emit_builtin_log(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) == 0)
        return pergyra_strdup("printf(\"\\n\")");

    if (ast_call_arg_count(call) == 1) {
        ASTNode *arg_node = ast_call_argument(call, 0);
        if (arg_node != NULL && arg_node->type == AST_STRING
            && ast_string_value(arg_node) != NULL) {
            const char *raw = ast_string_value(arg_node);
            bool multiline = (strchr(raw, '\n') != NULL)
                           || (strchr(raw, '\r') != NULL);
            char *escaped = escape_c_string_literal(ast_string_value(arg_node));
            if (escaped != NULL) {
                char *result;
                if (multiline) {
                    char *normalized = normalize_banner_string_literal(raw);
                    if (normalized == NULL)
                        normalized = pergyra_strdup(raw);
                    free(escaped);
                    escaped = escape_c_string_literal(normalized);
                    free(normalized);
                    if (escaped == NULL)
                        return pergyra_strdup("/* Log: failed to normalize string */");
                }
                result = strdup_fmt("pgy_log%s(\"%s\")",
                    multiline ? "_banner" : "", escaped);
                free(escaped);
                return result;
            }
            return pergyra_strdup("/* Log: failed to escape string */");
        }
        char *arg = emit_expression(ast_call_argument(call, 0), ctx);
        char *result = emit_c_log_call_for_expr(arg_node, arg, ctx);
        free(arg);
        return result;
    }

    CodeBuf *buf = codebuf_create();
    codebuf_write(buf, "do { ");
    for (size_t i = 0; i < ast_call_arg_count(call); i++) {
        ASTNode *arg_node = ast_call_argument(call, i);
        char *arg = emit_expression(arg_node, ctx);
        char *log_call = emit_c_log_call_for_expr(arg_node, arg, ctx);
        codebuf_write(buf, log_call);
        codebuf_write(buf, "; ");
        free(log_call);
        free(arg);
    }
    codebuf_write(buf, "} while(0)");
    char *result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}

char *
emit_builtin_log_raw(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) == 0)
        return pergyra_strdup("printf(\"\\n\")");

    if (ast_call_arg_count(call) == 1) {
        ASTNode *arg_node = ast_call_argument(call, 0);

        if (arg_node != NULL && arg_node->type == AST_STRING
            && ast_string_value(arg_node) != NULL) {
            char *escaped = escape_c_string_literal(ast_string_value(arg_node));
            if (escaped != NULL) {
                char *result = strdup_fmt("pgy_log(\"%s\")", escaped);
                free(escaped);
                return result;
            }
            return pergyra_strdup("/* LogRaw: failed to escape string */");
        }

        char *arg = emit_expression(arg_node, ctx);
        char *result = strdup_fmt("pgy_log(%s)", arg);
        free(arg);
        return result;
    }

    CodeBuf *buf = codebuf_create();
    codebuf_write(buf, "do { ");
    for (size_t i = 0; i < ast_call_arg_count(call); i++) {
        char *arg = emit_expression(ast_call_argument(call, i), ctx);
        codebuf_write(buf, "pgy_log(%s); ", arg);
        free(arg);
    }
    codebuf_write(buf, "} while(0)");
    char *result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}

char *
emit_builtin_log_banner(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 1 || ast_call_argument(call, 0) == NULL) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend: LogBanner requires one argument");
        return pergyra_strdup("0");
    }

    if (ast_call_arg_count(call) != 1)
        return emit_builtin_log(call, ctx);

    ASTNode *arg_node = ast_call_argument(call, 0);
    if (arg_node->type != AST_STRING)
        return emit_builtin_log(call, ctx);

    char *normalized = normalize_banner_string_literal(ast_string_value(arg_node));
    if (normalized == NULL)
        return emit_builtin_log(call, ctx);

    char *escaped = escape_c_string_literal(normalized);
    if (escaped == NULL) {
        free(normalized);
        return emit_builtin_log(call, ctx);
    }
    char *result = strdup_fmt("pgy_log_banner(\"%s\")", escaped);
    free(normalized);
    free(escaped);
    return result;
}
