/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type system implementation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "../common/string_compat.h"
#include "type_system.h"

/* -----------------------------------------------------------------
 * Per-analysis type ownership (WO-SEC-2)
 *
 * Types were never freed by anyone: they are reachable only through Symbols
 * and the resolution metadata table, both of which die with the
 * SemanticContext, so every Type a compile created became unreachable
 * garbage the moment analysis returned. Harmless for a batch compile that
 * then exits -- not harmless for pgy-lsp, which re-analyzes the document on
 * every keystroke and grew without bound. The sanitizer gate measured it at
 * 259 bytes for a five-line program.
 *
 * Every Type now comes from type_alloc(), which records it in the registry
 * for the analysis in flight. semantic_analyze_ex opens a registry, hands it
 * to the SemanticResult, and semantic_result_destroy frees it -- which is
 * after the whole pipeline, so an IR or backend that borrowed a Type* is
 * still reading live memory. Freeing each *allocation* once makes aliasing
 * (the same Type* held by many Symbols) a non-issue.
 *
 * The built-in singletons are allocated with tracking off: they are
 * process-scoped and type_system_shutdown owns them.
 * ----------------------------------------------------------------- */

typedef struct TypeRegistry
{
    Type  **types;
    size_t  count;
    size_t  capacity;
} TypeRegistry;

static TypeRegistry **
type_registry_active_slot(void)
{
    static _Thread_local TypeRegistry *active_registry;
    return &active_registry;
}

Type *
type_alloc(void)
{
    Type *type = calloc(1, sizeof(Type));
    TypeRegistry *active_registry = *type_registry_active_slot();

    if (type == NULL)
        return NULL;
    if (active_registry == NULL)
        return type;  /* singleton init, or an allocation outside an analysis */

    if (active_registry->count == active_registry->capacity) {
        size_t next = active_registry->capacity == 0
            ? 64
            : active_registry->capacity * 2;
        Type **grown;

        if (next > SIZE_MAX / sizeof(Type *)) {
            free(type);
            return NULL;
        }
        grown = realloc(active_registry->types, next * sizeof(Type *));
        if (grown == NULL) {
            /* Losing the registry slot would leak this Type silently, which
             * is the very thing being fixed. Fail the allocation instead. */
            free(type);
            return NULL;
        }
        active_registry->types = grown;
        active_registry->capacity = next;
    }
    active_registry->types[active_registry->count++] = type;
    return type;
}

TypeRegistry *
type_registry_begin(void)
{
    TypeRegistry *registry = calloc(1, sizeof(TypeRegistry));

    if (registry == NULL)
        return NULL;
    *type_registry_active_slot() = registry;
    return registry;
}

void
type_registry_end(TypeRegistry *registry)
{
    if (*type_registry_active_slot() == registry)
        *type_registry_active_slot() = NULL;
}

static void
type_free_owned(Type *type)
{
    if (type == NULL)
        return;

    /* Only what this Type allocated itself. The Type* elements inside these
     * arrays are separate registry entries and are freed on their own turn. */
    free(type->name);
    switch (type->kind) {
    case TYPE_KIND_GENERIC:
        free(type->data.generic.param_name);
        free(type->data.generic.constraints);
        break;
    case TYPE_KIND_CONSTRUCTED:
        free(type->data.constructed.args);
        break;
    case TYPE_KIND_FUNCTION:
        free(type->data.function.param_types);
        free(type->data.function.param_modes);
        free(type->data.function.param_escape_summary_masks);
        break;
    case TYPE_KIND_TUPLE:
        free(type->data.tuple.elements);
        break;
    case TYPE_KIND_PRIMITIVE:
    case TYPE_KIND_SLOT:
    case TYPE_KIND_CLASS:
    case TYPE_KIND_ENUM:
    case TYPE_KIND_TRAIT:
    case TYPE_KIND_ALIAS:
        break;
    }
    free(type);
}

void
type_registry_destroy(TypeRegistry *registry)
{
    if (registry == NULL)
        return;
    if (*type_registry_active_slot() == registry)
        *type_registry_active_slot() = NULL;
    for (size_t i = 0; i < registry->count; i++)
        type_free_owned(registry->types[i]);
    free(registry->types);
    free(registry);
}

/* -----------------------------------------------------------------
 * Built-in singleton types
 * ----------------------------------------------------------------- */

Type *TYPE_INT    = NULL;
Type *TYPE_LONG   = NULL;
Type *TYPE_DURATION = NULL;
Type *TYPE_FLOAT  = NULL;
Type *TYPE_DOUBLE = NULL;
Type *TYPE_BOOL   = NULL;
Type *TYPE_STRING = NULL;
Type *TYPE_PROJECTION = NULL; /* reflect result; compile-time, String runtime rep */
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
Type *TYPE_TEXT_BUILDER = NULL;
Type *TYPE_RESULT = NULL;
Type *TYPE_OPTION = NULL;

static void
type_free_singleton(Type **slot)
{
    Type *type;

    if (slot == NULL || *slot == NULL)
        return;
    type = *slot;
    free(type->name);
    free(type);
    *slot = NULL;
}

void
type_system_init(void)
{
    TypeRegistry *suspended;

    if (TYPE_INT != NULL)
        return;

    /*
     * The singletons belong to the process, not to any one analysis --
     * type_system_cleanup owns them. But this runs LAZILY, from inside
     * semantic_context_create, which means the registry for the analysis in
     * flight is already open. Left alone, the first analysis would adopt
     * TYPE_INT and friends and free them along with its result; the second
     * would read freed memory. (pgy survives that by doing exactly one
     * analysis per process. pgy-lsp, which re-analyzes on every keystroke,
     * would not -- and neither did the AIR battery, which is where the
     * sanitizer caught it.) So step outside the registry explicitly.
     */
    suspended = *type_registry_active_slot();
    *type_registry_active_slot() = NULL;

    TYPE_INT    = type_create_primitive("Int",    4, true);
    TYPE_LONG   = type_create_primitive("Long",   8, true);
    TYPE_DURATION = type_create_primitive("Duration", 8, true);
    TYPE_FLOAT  = type_create_primitive("Float",  4, false);
    TYPE_DOUBLE = type_create_primitive("Double", 8, false);
    TYPE_BOOL   = type_create_primitive("Bool",   1, false);
    TYPE_STRING = type_create_primitive("String", 0, false);
    TYPE_PROJECTION = type_create_primitive("projection", 0, false);
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
    TYPE_TEXT_BUILDER = type_create_primitive("TextBuilder", 0, false);
    TYPE_RESULT = type_create_primitive("Result", 0, false);
    TYPE_OPTION = type_create_primitive("Option", 0, false);

    *type_registry_active_slot() = suspended;
}

void
type_system_cleanup(void)
{
    type_free_singleton(&TYPE_INT);
    type_free_singleton(&TYPE_LONG);
    type_free_singleton(&TYPE_DURATION);
    type_free_singleton(&TYPE_FLOAT);
    type_free_singleton(&TYPE_DOUBLE);
    type_free_singleton(&TYPE_BOOL);
    type_free_singleton(&TYPE_STRING);
    type_free_singleton(&TYPE_QUBIT);
    type_free_singleton(&TYPE_VOID);
    type_free_singleton(&TYPE_UNKNOWN);
    type_free_singleton(&TYPE_ARRAY);
    type_free_singleton(&TYPE_SLICE);
    type_free_singleton(&TYPE_LIST);
    type_free_singleton(&TYPE_QUEUE);
    type_free_singleton(&TYPE_HASHMAP);
    type_free_singleton(&TYPE_SET);
    type_free_singleton(&TYPE_BOX);
    type_free_singleton(&TYPE_RC);
    type_free_singleton(&TYPE_WEAK);
    type_free_singleton(&TYPE_CHANNEL);
    type_free_singleton(&TYPE_FUTURE);
    type_free_singleton(&TYPE_REMOTE_FUTURE);
    type_free_singleton(&TYPE_TOKEN);
    type_free_singleton(&TYPE_DEVICE_SLOT);
    type_free_singleton(&TYPE_ALLOCATOR);
    type_free_singleton(&TYPE_TEXT_BUILDER);
    type_free_singleton(&TYPE_RESULT);
    type_free_singleton(&TYPE_OPTION);
}

/* -----------------------------------------------------------------
 * Constructors
 * ----------------------------------------------------------------- */

Type *
type_create_primitive(const char *name, size_t size, bool is_signed)
{
    Type *t = type_alloc();
    if (t == NULL)
        return NULL;

    t->kind                  = TYPE_KIND_PRIMITIVE;
    t->name                  = pergyra_strdup(name);
    if (t->name == NULL) {
        free(t);
        return NULL;
    }
    t->data.primitive.size   = size;
    t->data.primitive.is_signed = is_signed;
    return t;
}

Type *
type_create_generic(const char *param_name)
{
    Type *t = type_alloc();
    if (t == NULL)
        return NULL;

    t->kind                       = TYPE_KIND_GENERIC;
    t->name                       = pergyra_strdup(param_name);
    t->data.generic.param_name    = pergyra_strdup(param_name);
    if (t->name == NULL || t->data.generic.param_name == NULL) {
        free(t->name);
        free(t->data.generic.param_name);
        free(t);
        return NULL;
    }
    t->data.generic.constraints   = NULL;
    t->data.generic.constraint_count = 0;
    return t;
}

Type *
type_create_constructed(Type *constructor, Type **args, size_t arg_count)
{
    Type *t = type_alloc();
    if (t == NULL)
        return NULL;

    if (constructor == NULL || constructor->name == NULL) {
        free(t);
        return NULL;
    }
    if (arg_count > 0 && args == NULL) {
        free(t);
        return NULL;
    }

    /* Name: "Constructor<Arg0, Arg1, ...>" */
    size_t name_len = strlen(constructor->name);
    if (name_len > SIZE_MAX - 3) {
        free(t);
        return NULL;
    }
    name_len += 2; /* '<' '>' */
    for (size_t i = 0; i < arg_count; i++) {
        size_t arg_len;
        if (args[i] == NULL || args[i]->name == NULL) {
            free(t);
            return NULL;
        }
        arg_len = strlen(args[i]->name);
        if (name_len > SIZE_MAX - arg_len) {
            free(t);
            return NULL;
        }
        name_len += arg_len;
        if (i + 1 < arg_count) {
            if (name_len > SIZE_MAX - 2) {
                free(t);
                return NULL;
            }
            name_len += 2; /* ", " */
        }
    }
    if (name_len > SIZE_MAX - 1) {
        free(t);
        return NULL;
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
    if (arg_count > SIZE_MAX / sizeof(Type *)) {
        free(t->name);
        free(t);
        return NULL;
    }
    t->data.constructed.args = (arg_count > 0)
        ? malloc(arg_count * sizeof(Type *))
        : NULL;
    if (arg_count > 0 && t->data.constructed.args == NULL) {
        free(t->name);
        free(t);
        return NULL;
    }
    if (arg_count > 0)
        memcpy(t->data.constructed.args, args, arg_count * sizeof(Type *));
    return t;
}

Type *
type_constructed_constructor(const Type *type)
{
    if (type == NULL || type->kind != TYPE_KIND_CONSTRUCTED)
        return NULL;
    return type->data.constructed.constructor;
}

size_t
type_constructed_arg_count(const Type *type)
{
    if (type == NULL || type->kind != TYPE_KIND_CONSTRUCTED)
        return 0;
    return type->data.constructed.arg_count;
}

Type *
type_constructed_arg(const Type *type, size_t index)
{
    if (type == NULL || type->kind != TYPE_KIND_CONSTRUCTED)
        return NULL;
    if (index >= type->data.constructed.arg_count)
        return NULL;
    return type->data.constructed.args[index];
}

Type *
type_get_constructed_arg(const Type *type, size_t index)
{
    Type *arg = type_constructed_arg(type, index);
    return arg != NULL ? arg : TYPE_UNKNOWN;
}

bool
type_constructed_is(const Type *type, const Type *constructor, size_t arg_count)
{
    Type *actual_constructor = type_constructed_constructor(type);

    if (actual_constructor == NULL || constructor == NULL)
        return false;
    return type_constructed_arg_count(type) == arg_count
        && type_equals(actual_constructor, constructor);
}
