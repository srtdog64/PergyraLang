/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Ability reference display helpers for semantic contract diagnostics.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "../common/string_compat.h"
#include "type_checker_ability_ref_internal.h"

static char *
ability_ref_strdup_fmt(const char *fmt, ...)
{
    va_list ap;
    va_list ap2;
    int needed;
    char *buf;

    if (fmt == NULL)
        return NULL;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0) {
        va_end(ap2);
        return NULL;
    }

    buf = malloc((size_t)needed + 1);
    if (buf == NULL) {
        va_end(ap2);
        return NULL;
    }

    vsnprintf(buf, (size_t)needed + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

const char *
ability_ref_name(ASTNode *ability_ref)
{
    if (ability_ref == NULL || ability_ref->type != AST_TYPE)
        return NULL;
    return ast_type_name(ability_ref);
}

char *
ability_ref_display(ASTNode *ability_ref)
{
    if (ability_ref == NULL || ability_ref->type != AST_TYPE)
        return pergyra_strdup("<ability>");
    const char *ability_name = ast_type_name(ability_ref);
    GenericParams *generic_args = ast_type_generic_args(ability_ref);
    if (ability_name == NULL)
        return pergyra_strdup("<ability>");
    if (generic_args == NULL || generic_args->count == 0) {
        return pergyra_strdup(ability_name);
    }

    char *result = ability_ref_strdup_fmt("%s<", ability_name);
    if (result == NULL)
        return pergyra_strdup(ability_name);

    for (size_t i = 0; i < generic_args->count; i++) {
        GenericParam *gp = generic_args->params[i];
        ASTNode *arg = gp != NULL ? gp->constraint : NULL;
        char *arg_text = ability_ref_display(arg);
        char *next;
        if (i + 1 < generic_args->count) {
            next = ability_ref_strdup_fmt("%s%s, ", result,
                                          arg_text != NULL ? arg_text : "<type>");
        } else {
            next = ability_ref_strdup_fmt("%s%s>", result,
                                          arg_text != NULL ? arg_text : "<type>");
        }
        free(arg_text);
        free(result);
        result = next;
        if (result == NULL)
            return pergyra_strdup(ability_name);
    }

    return result;
}

char *
ability_decl_signature_display(const char *ability_name, GenericParams *params)
{
    char *result;

    if (ability_name == NULL)
        return pergyra_strdup("<ability>");
    if (params == NULL || params->count == 0)
        return pergyra_strdup(ability_name);

    result = ability_ref_strdup_fmt("%s<", ability_name);
    if (result == NULL)
        return pergyra_strdup(ability_name);

    for (size_t i = 0; i < params->count; i++) {
        GenericParam *gp = params->params[i];
        const char *param_name =
            (gp != NULL && gp->name != NULL) ? gp->name : "<type>";
        char *next;

        if (i + 1 < params->count)
            next = ability_ref_strdup_fmt("%s%s, ", result, param_name);
        else
            next = ability_ref_strdup_fmt("%s%s>", result, param_name);
        free(result);
        result = next;
        if (result == NULL)
            return pergyra_strdup(ability_name);
    }

    return result;
}

char *
ability_ref_effective_display(ASTNode *ability_decl, ASTNode *ability_ref)
{
    const char *ability_name;
    GenericParams *decl_params;
    GenericParams *provided_args;
    char *result;

    if (ability_ref == NULL || ability_ref->type != AST_TYPE)
        return ability_ref_display(ability_ref);

    ability_name = ast_type_name(ability_ref);
    if (ability_name == NULL)
        return ability_ref_display(ability_ref);

    if (ability_decl == NULL || ability_decl->type != AST_ABILITY_DECL)
        return ability_ref_display(ability_ref);

    decl_params = ast_ability_generic_params(ability_decl);
    if (decl_params == NULL || decl_params->count == 0)
        return pergyra_strdup(ability_name);

    provided_args = ast_type_generic_args(ability_ref);
    result = ability_ref_strdup_fmt("%s<", ability_name);
    if (result == NULL)
        return pergyra_strdup(ability_name);

    for (size_t i = 0; i < decl_params->count; i++) {
        GenericParam *decl_gp = decl_params->params[i];
        GenericParam *provided_gp =
            (provided_args != NULL && i < provided_args->count)
                ? provided_args->params[i]
                : NULL;
        ASTNode *arg_node =
            provided_gp != NULL && provided_gp->constraint != NULL
                ? provided_gp->constraint
                : (decl_gp != NULL ? decl_gp->default_type : NULL);
        char *arg_text = NULL;
        char *next;

        if (arg_node != NULL)
            arg_text = ability_ref_display(arg_node);
        if (arg_text == NULL && decl_gp != NULL && decl_gp->name != NULL)
            arg_text = pergyra_strdup(decl_gp->name);
        if (arg_text == NULL)
            arg_text = pergyra_strdup("<type>");

        if (i + 1 < decl_params->count) {
            next = ability_ref_strdup_fmt("%s%s, ", result,
                                          arg_text != NULL ? arg_text : "<type>");
        } else {
            next = ability_ref_strdup_fmt("%s%s>", result,
                                          arg_text != NULL ? arg_text : "<type>");
        }
        free(arg_text);
        free(result);
        result = next;
        if (result == NULL)
            return pergyra_strdup(ability_name);
    }

    return result;
}
