/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend declarator rendering for function and event-handler types.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "transpiler_type_declarator.h"
#include "transpiler_type_render.h"
#include "../common/string_compat.h"

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
    if (type_node == NULL)
        return declarator_strdup_fmt("void %s", name != NULL ? name : "value");

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        char *ret_type = pergyra_strdup("void");
        CodeBuf *params = codebuf_create();
        char *result;

        if (type_node->data.event_handler_type.return_type != NULL) {
            const char *rendered = pergyra_ast_type_to_c(
                type_node->data.event_handler_type.return_type);
            free(ret_type);
            ret_type = pergyra_strdup(rendered);
        }

        if (params == NULL) {
            result = declarator_strdup_fmt("%s (*%s)(void)", ret_type,
                name != NULL ? name : "value");
            free(ret_type);
            return result;
        }

        if (type_node->data.event_handler_type.param_count == 0) {
            codebuf_write(params, "void");
        } else {
            for (size_t i = 0; i < type_node->data.event_handler_type.param_count; i++) {
                if (i > 0)
                    codebuf_write(params, ", ");
                codebuf_write(params, "%s",
                    pergyra_ast_type_to_c(
                        type_node->data.event_handler_type.param_types[i]));
            }
        }

        result = declarator_strdup_fmt("%s (*%s)(%s)", ret_type,
            name != NULL ? name : "value", params->data);
        codebuf_destroy(params);
        free(ret_type);
        return result;
    }

    return declarator_strdup_fmt("%s %s", pergyra_ast_type_to_c(type_node),
        name != NULL ? name : "value");
}

char *
pergyra_func_pointer_declarator_from_decl(ASTNode *func_decl, const char *name)
{
    CodeBuf *params = NULL;
    char *ret_type = NULL;
    char *result = NULL;

    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL)
        return declarator_strdup_fmt("void (*%s)(void)",
            name != NULL ? name : "value");

    params = codebuf_create();
    ret_type = pergyra_strdup("void");
    if (func_decl->data.func_decl.return_type != NULL) {
        const char *rendered = pergyra_ast_type_to_c(func_decl->data.func_decl.return_type);
        free(ret_type);
        ret_type = pergyra_strdup(rendered);
    }

    if (params == NULL) {
        result = declarator_strdup_fmt("%s (*%s)(void)", ret_type,
            name != NULL ? name : "value");
        free(ret_type);
        return result;
    }

    if (func_decl->data.func_decl.param_count == 0) {
        codebuf_write(params, "void");
    } else {
        for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
            FuncParam *p = func_decl->data.func_decl.params[i];
            if (i > 0)
                codebuf_write(params, ", ");
            codebuf_write(params, "%s",
                p != NULL && p->type != NULL
                    ? pergyra_ast_type_to_c(p->type)
                    : "int32_t");
        }
    }

    result = declarator_strdup_fmt("%s (*%s)(%s)", ret_type,
        name != NULL ? name : "value", params->data);
    codebuf_destroy(params);
    free(ret_type);
    return result;
}

char *
pergyra_func_signature_declarator(ASTNode *return_type, const char *name,
                                  const char *params_sig)
{
    const char *fn_name = name != NULL ? name : "value";
    const char *sig = (params_sig != NULL && params_sig[0] != '\0')
        ? params_sig : "void";

    if (return_type != NULL && return_type->type == AST_EVENT_HANDLER_TYPE) {
        CodeBuf *handler_params = codebuf_create();
        char *ret_type = pergyra_strdup("void");
        char *result;

        if (return_type->data.event_handler_type.return_type != NULL) {
            const char *rendered = pergyra_ast_type_to_c(
                return_type->data.event_handler_type.return_type);
            free(ret_type);
            ret_type = pergyra_strdup(rendered);
        }

        if (handler_params == NULL) {
            result = declarator_strdup_fmt("%s (*%s(%s))(void)",
                ret_type, fn_name, sig);
            free(ret_type);
            return result;
        }

        if (return_type->data.event_handler_type.param_count == 0) {
            codebuf_write(handler_params, "void");
        } else {
            for (size_t i = 0; i < return_type->data.event_handler_type.param_count; i++) {
                if (i > 0)
                    codebuf_write(handler_params, ", ");
                codebuf_write(handler_params, "%s",
                    pergyra_ast_type_to_c(
                        return_type->data.event_handler_type.param_types[i]));
            }
        }

        result = declarator_strdup_fmt("%s (*%s(%s))(%s)", ret_type, fn_name, sig,
            handler_params->data);
        codebuf_destroy(handler_params);
        free(ret_type);
        return result;
    }

    return declarator_strdup_fmt("%s %s(%s)",
        pergyra_ast_type_to_c(return_type), fn_name, sig);
}
