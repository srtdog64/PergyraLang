/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type system implementation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../common/string_compat.h"
#include "type_system.h"

/* -----------------------------------------------------------------
 * Built-in singleton types
 * ----------------------------------------------------------------- */

Type *TYPE_INT    = NULL;
Type *TYPE_LONG   = NULL;
Type *TYPE_FLOAT  = NULL;
Type *TYPE_DOUBLE = NULL;
Type *TYPE_BOOL   = NULL;
Type *TYPE_STRING = NULL;
Type *TYPE_QUBIT = NULL;
Type *TYPE_VOID   = NULL;
Type *TYPE_UNKNOWN = NULL; /* Sentinel for error recovery */
Type *TYPE_ARRAY  = NULL;
Type *TYPE_SLICE  = NULL;
Type *TYPE_LIST   = NULL;
Type *TYPE_QUEUE  = NULL;
Type *TYPE_HASHMAP = NULL;
Type *TYPE_SET    = NULL;
Type *TYPE_BOX    = NULL;
Type *TYPE_RC     = NULL;
Type *TYPE_WEAK   = NULL;
Type *TYPE_CHANNEL = NULL;
Type *TYPE_FUTURE = NULL;
Type *TYPE_REMOTE_FUTURE = NULL;
Type *TYPE_TOKEN = NULL;
Type *TYPE_DEVICE_SLOT = NULL;
Type *TYPE_ALLOCATOR = NULL;
Type *TYPE_RESULT = NULL;
Type *TYPE_OPTION = NULL;

void
type_system_init(void)
{
    if (TYPE_INT != NULL)
        return;

    TYPE_INT    = type_create_primitive("Int",    4, true);
    TYPE_LONG   = type_create_primitive("Long",   8, true);
    TYPE_FLOAT  = type_create_primitive("Float",  4, false);
    TYPE_DOUBLE = type_create_primitive("Double", 8, false);
    TYPE_BOOL   = type_create_primitive("Bool",   1, false);
    TYPE_STRING = type_create_primitive("String", 0, false);
    TYPE_QUBIT  = type_create_primitive("QubitSlot", 4, false);
    TYPE_VOID   = type_create_primitive("Void",   0, false);
    TYPE_UNKNOWN = type_create_primitive("<unknown>", 0, false);
    TYPE_ARRAY  = type_create_primitive("Array",  0, false);
    TYPE_SLICE  = type_create_primitive("Slice",  0, false);
    TYPE_LIST   = type_create_primitive("List",   0, false);
    TYPE_QUEUE  = type_create_primitive("Queue",  0, false);
    TYPE_HASHMAP = type_create_primitive("HashMap", 0, false);
    TYPE_SET    = type_create_primitive("Set",    0, false);
    TYPE_BOX    = type_create_primitive("Box",    0, false);
    TYPE_RC     = type_create_primitive("Rc",     0, false);
    TYPE_WEAK   = type_create_primitive("Weak",   0, false);
    TYPE_CHANNEL = type_create_primitive("Channel", 0, false);
    TYPE_FUTURE = type_create_primitive("Future", 0, false);
    TYPE_REMOTE_FUTURE = type_create_primitive("RemoteFuture", 0, false);
    TYPE_TOKEN = type_create_primitive("Token", 0, false);
    TYPE_DEVICE_SLOT = type_create_primitive("DeviceSlot", 0, false);
    TYPE_ALLOCATOR = type_create_primitive("Allocator", 0, false);
    TYPE_RESULT = type_create_primitive("Result", 0, false);
    TYPE_OPTION = type_create_primitive("Option", 0, false);
}

void
type_system_cleanup(void)
{
    free(TYPE_INT->name);    free(TYPE_INT);
    free(TYPE_LONG->name);   free(TYPE_LONG);
    free(TYPE_FLOAT->name);  free(TYPE_FLOAT);
    free(TYPE_DOUBLE->name); free(TYPE_DOUBLE);
    free(TYPE_BOOL->name);   free(TYPE_BOOL);
    free(TYPE_STRING->name); free(TYPE_STRING);
    free(TYPE_QUBIT->name);  free(TYPE_QUBIT);
    free(TYPE_VOID->name);   free(TYPE_VOID);
    free(TYPE_UNKNOWN->name);free(TYPE_UNKNOWN);
    free(TYPE_ARRAY->name);  free(TYPE_ARRAY);
    free(TYPE_SLICE->name);  free(TYPE_SLICE);
    free(TYPE_LIST->name);   free(TYPE_LIST);
    free(TYPE_QUEUE->name);  free(TYPE_QUEUE);
    free(TYPE_HASHMAP->name); free(TYPE_HASHMAP);
    free(TYPE_SET->name);    free(TYPE_SET);
    free(TYPE_BOX->name);    free(TYPE_BOX);
    free(TYPE_RC->name);     free(TYPE_RC);
    free(TYPE_WEAK->name);   free(TYPE_WEAK);
    free(TYPE_CHANNEL->name); free(TYPE_CHANNEL);
    free(TYPE_FUTURE->name); free(TYPE_FUTURE);
    free(TYPE_REMOTE_FUTURE->name); free(TYPE_REMOTE_FUTURE);
    free(TYPE_TOKEN->name); free(TYPE_TOKEN);
    free(TYPE_DEVICE_SLOT->name); free(TYPE_DEVICE_SLOT);
    free(TYPE_ALLOCATOR->name); free(TYPE_ALLOCATOR);
    free(TYPE_RESULT->name); free(TYPE_RESULT);
    free(TYPE_OPTION->name); free(TYPE_OPTION);

    TYPE_INT = TYPE_LONG = TYPE_FLOAT = TYPE_DOUBLE =
    TYPE_BOOL = TYPE_STRING = TYPE_QUBIT = TYPE_VOID = TYPE_UNKNOWN =
    TYPE_ARRAY = TYPE_SLICE = TYPE_LIST = TYPE_QUEUE = TYPE_HASHMAP = TYPE_SET = TYPE_BOX = TYPE_RC =
        TYPE_WEAK = TYPE_CHANNEL = TYPE_FUTURE = TYPE_REMOTE_FUTURE =
        TYPE_TOKEN = TYPE_DEVICE_SLOT = TYPE_ALLOCATOR = TYPE_RESULT = TYPE_OPTION = NULL;
}

/* -----------------------------------------------------------------
 * Constructors
 * ----------------------------------------------------------------- */

Type *
type_create_primitive(const char *name, size_t size, bool is_signed)
{
    Type *t = calloc(1, sizeof(Type));
    if (t == NULL)
        return NULL;

    t->kind                  = TYPE_KIND_PRIMITIVE;
    t->name                  = pergyra_strdup(name);
    t->data.primitive.size   = size;
    t->data.primitive.is_signed = is_signed;
    return t;
}

Type *
type_create_generic(const char *param_name)
{
    Type *t = calloc(1, sizeof(Type));
    if (t == NULL)
        return NULL;

    t->kind                       = TYPE_KIND_GENERIC;
    t->name                       = pergyra_strdup(param_name);
    t->data.generic.param_name    = pergyra_strdup(param_name);
    t->data.generic.constraints   = NULL;
    t->data.generic.constraint_count = 0;
    return t;
}

Type *
type_create_constructed(Type *constructor, Type **args, size_t arg_count)
{
    Type *t = calloc(1, sizeof(Type));
    if (t == NULL)
        return NULL;

    /* Name: "Constructor<Arg0, Arg1, ...>" */
    size_t name_len = strlen(constructor->name) + 2; /* '<' '>' */
    for (size_t i = 0; i < arg_count; i++) {
        name_len += strlen(args[i]->name);
        if (i + 1 < arg_count)
            name_len += 2; /* ", " */
    }
    name_len += 1; /* '\0' */

    t->name = malloc(name_len);
    if (t->name == NULL) {
        free(t);
        return NULL;
    }
    {
        size_t offset = 0;
        size_t constructor_len = strlen(constructor->name);
        memcpy(t->name + offset, constructor->name, constructor_len);
        offset += constructor_len;
        t->name[offset++] = '<';
        for (size_t i = 0; i < arg_count; i++) {
            size_t arg_len = strlen(args[i]->name);
            memcpy(t->name + offset, args[i]->name, arg_len);
            offset += arg_len;
            if (i + 1 < arg_count) {
                t->name[offset++] = ',';
                t->name[offset++] = ' ';
            }
        }
        t->name[offset++] = '>';
        t->name[offset] = '\0';
    }

    t->kind = TYPE_KIND_CONSTRUCTED;
    t->data.constructed.constructor = constructor;
    t->data.constructed.arg_count   = arg_count;
    t->data.constructed.args = malloc(arg_count * sizeof(Type *));
    if (t->data.constructed.args == NULL) {
        free(t->name);
        free(t);
        return NULL;
    }
    memcpy(t->data.constructed.args, args, arg_count * sizeof(Type *));
    return t;
}

Type *
type_create_function(Type **params, size_t param_count, Type *return_type)
{
    Type *t = calloc(1, sizeof(Type));
    if (t == NULL)
        return NULL;

    t->kind = TYPE_KIND_FUNCTION;

    /* Name: "(P0, P1) -> R" */
    size_t name_len = 3; /* "()" + "->" overhead */
    for (size_t i = 0; i < param_count; i++) {
        name_len += strlen(params[i]->name) + 2;
    }
    name_len += strlen(return_type->name) + 5;

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
    t->data.function.param_types  = (param_count > 0)
        ? calloc(param_count, sizeof(Type *))
        : NULL;
    if (param_count > 0 && t->data.function.param_types == NULL) {
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
type_create_tuple(Type **elements, size_t element_count)
{
    Type *t = calloc(1, sizeof(Type));
    if (t == NULL)
        return NULL;

    t->kind = TYPE_KIND_TUPLE;

    /* Name: "(T0, T1, T2)" */
    size_t name_len = 3; /* "()" + '\0' */
    for (size_t i = 0; i < element_count; i++) {
        const char *en = (elements[i] != NULL && elements[i]->name != NULL)
                            ? elements[i]->name : "?";
        name_len += strlen(en);
        if (i + 1 < element_count)
            name_len += 2; /* ", " */
    }

    t->name = malloc(name_len);
    if (t->name == NULL) {
        free(t);
        return NULL;
    }
    {
        size_t offset = 0;
        t->name[offset++] = '(';
        for (size_t i = 0; i < element_count; i++) {
            const char *en = (elements[i] != NULL && elements[i]->name != NULL)
                                ? elements[i]->name : "?";
            size_t elen = strlen(en);
            memcpy(t->name + offset, en, elen);
            offset += elen;
            if (i + 1 < element_count) {
                t->name[offset++] = ',';
                t->name[offset++] = ' ';
            }
        }
        t->name[offset++] = ')';
        t->name[offset] = '\0';
    }

    t->data.tuple.element_count = element_count;
    t->data.tuple.elements = (element_count > 0)
        ? calloc(element_count, sizeof(Type *))
        : NULL;
    if (element_count > 0 && t->data.tuple.elements == NULL) {
        free(t->name);
        free(t);
        return NULL;
    }
    if (element_count > 0 && elements != NULL)
        memcpy(t->data.tuple.elements, elements, element_count * sizeof(Type *));
    return t;
}

bool
type_is_tuple(const Type *t)
{
    return t != NULL && t->kind == TYPE_KIND_TUPLE;
}

size_t
type_tuple_arity(const Type *t)
{
    if (!type_is_tuple(t))
        return 0;
    return t->data.tuple.element_count;
}

Type *
type_tuple_get_element(const Type *t, size_t index)
{
    if (!type_is_tuple(t))
        return NULL;
    if (index >= t->data.tuple.element_count)
        return NULL;
    return t->data.tuple.elements[index];
}

Type *
type_create_slot(Type *inner_type, bool is_secure)
{
    return type_create_slot_access(inner_type, is_secure, SLOT_ACCESS_OWNED);
}

Type *
type_create_slot_access(Type *inner_type, bool is_secure, SlotAccessMode access_mode)
{
    Type *t;
    const char *prefix = "Slot<";
    size_t prefix_len;
    size_t inner_len;
    size_t name_len;

    if (inner_type == NULL || inner_type->name == NULL)
        return NULL;

    t = calloc(1, sizeof(Type));
    if (t == NULL)
        return NULL;

    t->kind = TYPE_KIND_SLOT;

    if (access_mode == SLOT_ACCESS_READ_VIEW)
        prefix = "ReadView<";
    else if (access_mode == SLOT_ACCESS_WRITE_VIEW)
        prefix = "WriteView<";
    else if (access_mode == SLOT_ACCESS_MOVE_TOKEN)
        prefix = "MoveToken<";
    else if (is_secure)
        prefix = "SecureSlot<";

    prefix_len = strlen(prefix);
    inner_len = strlen(inner_type->name);
    if (inner_len > ((size_t)-1) - prefix_len - 2) {
        free(t);
        return NULL;
    }
    name_len = prefix_len + inner_len + 2;
    t->name = malloc(name_len);
    if (t->name == NULL) {
        free(t);
        return NULL;
    }
    {
        size_t offset = 0;
        memcpy(t->name + offset, prefix, prefix_len);
        offset += prefix_len;
        memcpy(t->name + offset, inner_type->name, inner_len);
        offset += inner_len;
        t->name[offset++] = '>';
        t->name[offset] = '\0';
    }

    t->data.slot.inner_type    = inner_type;
    t->data.slot.is_secure     = is_secure;
    t->data.slot.security_level = 0;
    t->data.slot.access_mode   = access_mode;
    return t;
}

Type *
type_create_read_view(Type *inner_type)
{
    return type_create_slot_access(inner_type, false, SLOT_ACCESS_READ_VIEW);
}

Type *
type_create_write_view(Type *inner_type)
{
    return type_create_slot_access(inner_type, false, SLOT_ACCESS_WRITE_VIEW);
}
