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
#include "transpiler_type_render.h"
#include "../common/string_compat.h"

static bool
declarator_ast_type_to_c_copy(ASTNode *type_node, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0)
        return false;
    return pergyra_ast_type_to_c_copy(type_node, out, out_size);
}

static char *
declarator_strdup_fmt(const char *fmt, ...)
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
        return NULL;
    }

    buf = malloc((size_t)n + 1);
    if (buf == NULL) {
        va_end(ap2);
        return NULL;
    }

    vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

char *
pergyra_ast_typed_declarator(ASTNode *type_node, const char *name)
{
    char type_buf[256];

    if (type_node == NULL)
        return declarator_strdup_fmt("void %s", name != NULL ? name : "value");

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        char ret_type_buf[256];
        const char *ret_type = "void";
        CodeBuf *params = codebuf_create();
        char *result;

        ASTNode *return_type = ast_event_handler_return_type(type_node);
        if (return_type != NULL) {
            if (declarator_ast_type_to_c_copy(
                    return_type,
                    ret_type_buf, sizeof(ret_type_buf)))
                ret_type = ret_type_buf;
        }

        if (params == NULL) {
            result = declarator_strdup_fmt("%s (*%s)(void)", ret_type,
                name != NULL ? name : "value");
            return result;
        }

        size_t param_count = ast_event_handler_param_count(type_node);
        if (param_count == 0) {
            codebuf_write(params, "void");
        } else {
            for (size_t i = 0; i < param_count; i++) {
                char param_buf[256];
                if (i > 0)
                    codebuf_write(params, ", ");
                if (declarator_ast_type_to_c_copy(
                        ast_event_handler_param_type(type_node, i),
                        param_buf, sizeof(param_buf))) {
                    codebuf_write(params, "%s", param_buf);
                } else {
                    codebuf_write(params, "void *");
                }
            }
        }

        result = declarator_strdup_fmt("%s (*%s)(%s)", ret_type,
            name != NULL ? name : "value", params->data);
        codebuf_destroy(params);
        return result;
    }

    if (!declarator_ast_type_to_c_copy(type_node, type_buf, sizeof(type_buf)))
        type_buf[0] = '\0';
    return declarator_strdup_fmt("%s %s", type_buf[0] != '\0' ? type_buf : "void *",
        name != NULL ? name : "value");
}

char *
pergyra_func_pointer_declarator_from_decl(ASTNode *func_decl, const char *name)
{
    CodeBuf *params = NULL;
    char ret_type_buf[256];
    const char *ret_type = "void";
    char *result = NULL;

    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL)
        return declarator_strdup_fmt("void (*%s)(void)",
            name != NULL ? name : "value");

    params = codebuf_create();
    ASTNode *return_type = ast_func_return_type(func_decl);
    if (return_type != NULL) {
        if (declarator_ast_type_to_c_copy(return_type,
                                          ret_type_buf,
                                          sizeof(ret_type_buf)))
            ret_type = ret_type_buf;
    }

    if (params == NULL) {
        result = declarator_strdup_fmt("%s (*%s)(void)", ret_type,
            name != NULL ? name : "value");
        return result;
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
            codebuf_write(params, "%s",
                p != NULL
                    && p->type != NULL
                    && declarator_ast_type_to_c_copy(p->type,
                        param_buf,
                        sizeof(param_buf))
                    ? param_buf
                    : "int32_t");
        }
    }

    result = declarator_strdup_fmt("%s (*%s)(%s)", ret_type,
        name != NULL ? name : "value", params->data);
    codebuf_destroy(params);
    return result;
}

char *
pergyra_func_signature_declarator(ASTNode *return_type, const char *name,
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
            if (declarator_ast_type_to_c_copy(
                    handler_return_type,
                    ret_type_buf, sizeof(ret_type_buf)))
                ret_type = ret_type_buf;
        }

        if (handler_params == NULL) {
            result = declarator_strdup_fmt("%s (*%s(%s))(void)",
                ret_type, fn_name, sig);
            return result;
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
                if (declarator_ast_type_to_c_copy(
                        ast_event_handler_param_type(return_type, i),
                        param_buf, sizeof(param_buf))) {
                    codebuf_write(handler_params, "%s", param_buf);
                } else {
                    codebuf_write(handler_params, "void *");
                }
            }
        }

        result = declarator_strdup_fmt("%s (*%s(%s))(%s)", ret_type, fn_name, sig,
            handler_params->data);
        codebuf_destroy(handler_params);
        return result;
    }

    if (!declarator_ast_type_to_c_copy(return_type, return_type_buf,
                                       sizeof(return_type_buf))) {
        memcpy(return_type_buf, "void", sizeof("void"));
    }
    return declarator_strdup_fmt("%s %s(%s)",
        return_type_buf, fn_name, sig);
}
