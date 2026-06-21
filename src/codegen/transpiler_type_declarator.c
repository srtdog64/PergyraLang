/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend declarator rendering for function and event-handler types.
 */

#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transpiler_type_declarator.h"
#include "transpiler_context.h"
#include "transpiler_type_require.h"
#include "transpiler_type_render.h"
#include "../common/string_compat.h"
#include "../semantic/diag_codes.h"

static bool
declarator_ast_type_to_c_copy_in_ctx(TranspilerCtx *ctx, ASTNode *type_node,
                                     char *out, size_t out_size)
{
    if (out == NULL || out_size == 0)
        return false;
    return pergyra_ast_type_to_c_copy_in_ctx(ctx, type_node, out, out_size);
}

static char *
declarator_heap_fmt(TranspilerCtx *ctx, const char *fmt, ...)
{
    va_list ap;
    va_list ap2;
    int n;
    char *buf;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (n < 0) {
        va_end(ap2);
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend declarator formatting failed");
        return NULL;
    }

    buf = malloc((size_t)n + 1);
    if (buf == NULL) {
        va_end(ap2);
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend declarator allocation failed");
        return NULL;
    }

    vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

static bool
declarator_require_ast_c_type_copy(TranspilerCtx *ctx,
                                   ASTNode *type_node,
                                   const char *surface,
                                   char *out,
                                   size_t out_size)
{
    if (declarator_ast_type_to_c_copy_in_ctx(ctx, type_node, out, out_size))
        return true;
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "C backend declarator cannot render C type for %s",
        surface != NULL ? surface : "typed declaration");
    return false;
}

char *
pergyra_ast_typed_declarator_in_ctx(TranspilerCtx *ctx,
                                    ASTNode *type_node,
                                    const char *name)
{
    char type_buf[256];

    if (type_node == NULL)
        return declarator_heap_fmt(ctx, "void %s",
            name != NULL ? name : "value");

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        char ret_type_buf[256];
        const char *ret_type = "void";
        CodeBuf *params = codebuf_create();
        char *result;

        ASTNode *return_type = ast_event_handler_return_type(type_node);
        if (return_type != NULL) {
            if (declarator_require_ast_c_type_copy(ctx,
                    return_type,
                    "event handler return type",
                    ret_type_buf, sizeof(ret_type_buf)))
                ret_type = ret_type_buf;
            else {
                codebuf_destroy(params);
                return NULL;
            }
        }

        if (params == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend declarator parameter buffer allocation failed");
            return NULL;
        }

        size_t param_count = ast_event_handler_param_count(type_node);
        if (param_count == 0) {
            codebuf_write(params, "void");
        } else {
            for (size_t i = 0; i < param_count; i++) {
                char param_buf[256];
                if (i > 0)
                    codebuf_write(params, ", ");
                if (declarator_require_ast_c_type_copy(ctx,
                        ast_event_handler_param_type(type_node, i),
                        "event handler parameter type",
                        param_buf, sizeof(param_buf))) {
                    codebuf_write(params, "%s", param_buf);
                } else {
                    codebuf_destroy(params);
                    return NULL;
                }
            }
        }

        result = declarator_heap_fmt(ctx, "%s (*%s)(%s)", ret_type,
            name != NULL ? name : "value", params->data);
        codebuf_destroy(params);
        return result;
    }

    if (!declarator_require_ast_c_type_copy(ctx, type_node,
            "typed declarator", type_buf, sizeof(type_buf)))
        return NULL;
    return declarator_heap_fmt(ctx, "%s %s", type_buf,
        name != NULL ? name : "value");
}

char *
pergyra_ast_typed_declarator(ASTNode *type_node, const char *name)
{
    return pergyra_ast_typed_declarator_in_ctx(NULL, type_node, name);
}

char *
pergyra_func_pointer_declarator_from_decl_in_ctx(TranspilerCtx *ctx,
                                                 ASTNode *func_decl,
                                                 const char *name)
{
    CodeBuf *params = NULL;
    char ret_type_buf[256];
    const char *ret_type = "void";
    char *result = NULL;

    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL)
        return declarator_heap_fmt(ctx, "void (*%s)(void)",
            name != NULL ? name : "value");

    params = codebuf_create();
    ASTNode *return_type = ast_func_return_type(func_decl);
    if (return_type != NULL) {
        if (declarator_require_ast_c_type_copy(ctx,
                return_type, "function pointer return type",
                ret_type_buf, sizeof(ret_type_buf)))
            ret_type = ret_type_buf;
        else {
            codebuf_destroy(params);
            return NULL;
        }
    }

    if (params == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend declarator parameter buffer allocation failed");
        return NULL;
    }

    size_t param_count = ast_func_param_count(func_decl);
    if (param_count == 0) {
        codebuf_write(params, "void");
    } else {
        for (size_t i = 0; i < param_count; i++) {
            FuncParam *p = ast_func_param(func_decl, i);
            char param_buf[256];
            if (i > 0)
                codebuf_write(params, ", ");
            if (p == NULL || p->type == NULL
                || !declarator_require_ast_c_type_copy(ctx,
                    p->type, "function pointer parameter type",
                    param_buf, sizeof(param_buf))) {
                codebuf_destroy(params);
                return NULL;
            }
            codebuf_write(params, "%s", param_buf);
        }
    }

    result = declarator_heap_fmt(ctx, "%s (*%s)(%s)", ret_type,
        name != NULL ? name : "value", params->data);
    codebuf_destroy(params);
    return result;
}

char *
pergyra_func_pointer_declarator_from_decl(ASTNode *func_decl, const char *name)
{
    return pergyra_func_pointer_declarator_from_decl_in_ctx(
        NULL, func_decl, name);
}

char *
pergyra_func_pointer_declarator_from_type_names_in_ctx(
    TranspilerCtx *ctx,
    const char *return_type_name,
    size_t param_count,
    char *const *param_type_names,
    const char *name)
{
    CodeBuf *params = NULL;
    char ret_type_buf[256];
    const char *ret_type = NULL;
    char *result = NULL;

    if (!transpiler_require_type_name_c_type_copy(ctx, return_type_name,
            "function pointer return type", ret_type_buf,
            sizeof(ret_type_buf))) {
        return NULL;
    }
    ret_type = ret_type_buf;

    params = codebuf_create();
    if (params == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend declarator parameter buffer allocation failed");
        return NULL;
    }

    if (param_count == 0) {
        codebuf_write(params, "void");
    } else {
        for (size_t i = 0; i < param_count; i++) {
            char param_buf[256];
            if (i > 0)
                codebuf_write(params, ", ");
            if (param_type_names == NULL
                || param_type_names[i] == NULL
                || !transpiler_require_type_name_c_type_copy(ctx,
                    param_type_names[i],
                    "function pointer parameter type",
                    param_buf, sizeof(param_buf))) {
                codebuf_destroy(params);
                return NULL;
            }
            codebuf_write(params, "%s", param_buf);
        }
    }

    result = declarator_heap_fmt(ctx, "%s (*%s)(%s)", ret_type,
        name != NULL ? name : "value", params->data);
    codebuf_destroy(params);
    return result;
}

char *
pergyra_func_signature_declarator_in_ctx(TranspilerCtx *ctx,
                                         ASTNode *return_type,
                                         const char *name,
                                         const char *params_sig)
{
    const char *fn_name = name != NULL ? name : "value";
    const char *sig = (params_sig != NULL && params_sig[0] != '\0')
        ? params_sig : "void";
    char return_type_buf[256];

    if (return_type != NULL && return_type->type == AST_EVENT_HANDLER_TYPE) {
        CodeBuf *handler_params = codebuf_create();
        char ret_type_buf[256];
        const char *ret_type = "void";
        char *result;

        ASTNode *handler_return_type =
            ast_event_handler_return_type(return_type);
        if (handler_return_type != NULL) {
            if (declarator_require_ast_c_type_copy(ctx,
                    handler_return_type,
                    "returned event handler return type",
                    ret_type_buf, sizeof(ret_type_buf)))
                ret_type = ret_type_buf;
            else {
                codebuf_destroy(handler_params);
                return NULL;
            }
        }

        if (handler_params == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend declarator parameter buffer allocation failed");
            return NULL;
        }

        size_t handler_param_count =
            ast_event_handler_param_count(return_type);
        if (handler_param_count == 0) {
            codebuf_write(handler_params, "void");
        } else {
            for (size_t i = 0; i < handler_param_count; i++) {
                char param_buf[256];
                if (i > 0)
                    codebuf_write(handler_params, ", ");
                if (declarator_require_ast_c_type_copy(ctx,
                        ast_event_handler_param_type(return_type, i),
                        "returned event handler parameter type",
                        param_buf, sizeof(param_buf))) {
                    codebuf_write(handler_params, "%s", param_buf);
                } else {
                    codebuf_destroy(handler_params);
                    return NULL;
                }
            }
        }

        result = declarator_heap_fmt(ctx, "%s (*%s(%s))(%s)", ret_type,
            fn_name, sig, handler_params->data);
        codebuf_destroy(handler_params);
        return result;
    }

    if (return_type == NULL) {
        memcpy(return_type_buf, "void", sizeof("void"));
    } else if (!declarator_require_ast_c_type_copy(ctx, return_type,
            "function return type",
            return_type_buf, sizeof(return_type_buf))) {
        return NULL;
    }
    return declarator_heap_fmt(ctx, "%s %s(%s)",
        return_type_buf, fn_name, sig);
}

char *
pergyra_func_signature_declarator(ASTNode *return_type, const char *name,
                                  const char *params_sig)
{
    return pergyra_func_signature_declarator_in_ctx(
        NULL, return_type, name, params_sig);
}
