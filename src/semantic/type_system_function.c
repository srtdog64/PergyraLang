/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Function type construction and signature fact access.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "type_system.h"

Type *
type_create_function(Type **params, size_t param_count, Type *return_type)
{
    Type *t = type_alloc();
    if (t == NULL)
        return NULL;

    t->kind = TYPE_KIND_FUNCTION;
    if (return_type == NULL || return_type->name == NULL
        || (param_count > 0 && params == NULL)) {
        free(t);
        return NULL;
    }

    /* Name: "(P0, P1) -> R" */
    size_t name_len = 3; /* "()" + "->" overhead */
    for (size_t i = 0; i < param_count; i++) {
        size_t param_len;
        if (params[i] == NULL || params[i]->name == NULL) {
            free(t);
            return NULL;
        }
        param_len = strlen(params[i]->name);
        if (name_len > SIZE_MAX - param_len - 2) {
            free(t);
            return NULL;
        }
        name_len += param_len + 2;
    }
    {
        size_t ret_len = strlen(return_type->name);
        if (name_len > SIZE_MAX - ret_len
            || name_len + ret_len > SIZE_MAX - 5) {
            free(t);
            return NULL;
        }
        name_len += ret_len + 5;
    }

    t->name = malloc(name_len);
    if (t->name == NULL) {
        free(t);
        return NULL;
    }
    {
        size_t offset = 0;
        t->name[offset++] = '(';
        for (size_t i = 0; i < param_count; i++) {
            size_t param_len = strlen(params[i]->name);
            memcpy(t->name + offset, params[i]->name, param_len);
            offset += param_len;
            if (i + 1 < param_count) {
                t->name[offset++] = ',';
                t->name[offset++] = ' ';
            }
        }
        t->name[offset++] = ')';
        t->name[offset++] = ' ';
        t->name[offset++] = '-';
        t->name[offset++] = '>';
        t->name[offset++] = ' ';
        {
            size_t ret_len = strlen(return_type->name);
            memcpy(t->name + offset, return_type->name, ret_len);
            offset += ret_len;
        }
        t->name[offset] = '\0';
    }

    t->data.function.return_type  = return_type;
    t->data.function.param_count  = param_count;
    t->data.function.effect_mask  = EFFECT_NONE;
    t->data.function.body_summary_mask = BODY_SUMMARY_NONE;
    t->data.function.has_body_summary_facts = false;
    t->data.function.has_param_escape_summary_facts = false;
    t->data.function.param_types  = (param_count > 0)
        ? calloc(param_count, sizeof(Type *))
        : NULL;
    if (param_count > 0 && t->data.function.param_types == NULL) {
        free(t->name);
        free(t);
        return NULL;
    }
    t->data.function.param_modes  = (param_count > 0)
        ? calloc(param_count, sizeof(ParamMode))
        : NULL;
    if (param_count > 0 && t->data.function.param_modes == NULL) {
        free(t->data.function.param_types);
        free(t->name);
        free(t);
        return NULL;
    }
    t->data.function.param_escape_summary_masks = (param_count > 0)
        ? calloc(param_count, sizeof(uint32_t))
        : NULL;
    if (param_count > 0
        && t->data.function.param_escape_summary_masks == NULL) {
        free(t->data.function.param_modes);
        free(t->data.function.param_types);
        free(t->name);
        free(t);
        return NULL;
    }
    if (param_count > 0 && params != NULL) {
        memcpy(t->data.function.param_types, params,
               param_count * sizeof(Type *));
    }
    return t;
}

Type *
type_function_return_type(const Type *type)
{
    if (type == NULL || type->kind != TYPE_KIND_FUNCTION)
        return NULL;
    return type->data.function.return_type;
}

void
type_function_set_return_type(Type *type, Type *return_type)
{
    if (type == NULL || type->kind != TYPE_KIND_FUNCTION)
        return;
    type->data.function.return_type = return_type;
}

size_t
type_function_param_count(const Type *type)
{
    if (type == NULL || type->kind != TYPE_KIND_FUNCTION)
        return 0;
    return type->data.function.param_count;
}

Type *
type_function_param_type(const Type *type, size_t index)
{
    if (type == NULL || type->kind != TYPE_KIND_FUNCTION)
        return NULL;
    if (index >= type->data.function.param_count)
        return NULL;
    return type->data.function.param_types[index];
}

ParamMode
type_function_param_mode(const Type *type, size_t index)
{
    if (type == NULL || type->kind != TYPE_KIND_FUNCTION)
        return PARAM_MODE_DEFAULT;
    if (index >= type->data.function.param_count)
        return PARAM_MODE_DEFAULT;
    if (type->data.function.param_modes == NULL)
        return PARAM_MODE_DEFAULT;
    return type->data.function.param_modes[index];
}

void
type_function_set_param_mode(Type *type, size_t index, ParamMode mode)
{
    if (type == NULL || type->kind != TYPE_KIND_FUNCTION)
        return;
    if (index >= type->data.function.param_count)
        return;
    if (type->data.function.param_modes == NULL)
        return;
    type->data.function.param_modes[index] = mode;
}

uint32_t
type_function_param_escape_summary(const Type *type, size_t index)
{
    if (type == NULL || type->kind != TYPE_KIND_FUNCTION)
        return 0;
    if (index >= type->data.function.param_count)
        return 0;
    if (type->data.function.param_escape_summary_masks == NULL)
        return 0;
    return type->data.function.param_escape_summary_masks[index];
}

bool
type_function_has_param_escape_summary(const Type *type, size_t index)
{
    return type != NULL
        && type->kind == TYPE_KIND_FUNCTION
        && type->data.function.has_param_escape_summary_facts
        && index < type->data.function.param_count
        && type->data.function.param_escape_summary_masks != NULL;
}

void
type_function_set_param_escape_summary(Type *type, size_t index,
                                       uint32_t summary)
{
    if (type == NULL || type->kind != TYPE_KIND_FUNCTION)
        return;
    if (index >= type->data.function.param_count)
        return;
    if (type->data.function.param_escape_summary_masks == NULL)
        return;
    type->data.function.param_escape_summary_masks[index] = summary;
}

void
type_function_finish_param_escape_summaries(Type *type)
{
    if (type == NULL || type->kind != TYPE_KIND_FUNCTION)
        return;
    if (type->data.function.param_count > 0
        && type->data.function.param_escape_summary_masks == NULL)
        return;
    type->data.function.has_param_escape_summary_facts = true;
}

void
type_function_set_effects(Type *type, uint32_t effect_mask)
{
    if (type == NULL || type->kind != TYPE_KIND_FUNCTION)
        return;
    type->data.function.effect_mask = effect_mask;
}

void
type_function_set_capabilities(Type *type, uint32_t capability_mask)
{
    if (type == NULL || type->kind != TYPE_KIND_FUNCTION)
        return;
    type->data.function.capability_mask = capability_mask;
}

void
type_function_set_body_summary(Type *type, uint32_t body_summary_mask)
{
    if (type == NULL || type->kind != TYPE_KIND_FUNCTION)
        return;
    type->data.function.body_summary_mask = body_summary_mask;
    type->data.function.has_body_summary_facts = true;
}
