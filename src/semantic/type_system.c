/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type system implementation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
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
Type *TYPE_VOID   = NULL;
Type *TYPE_UNKNOWN = NULL; /* Sentinel for error recovery */

void
type_system_init(void)
{
    TYPE_INT    = type_create_primitive("Int",    4, true);
    TYPE_LONG   = type_create_primitive("Long",   8, true);
    TYPE_FLOAT  = type_create_primitive("Float",  4, false);
    TYPE_DOUBLE = type_create_primitive("Double", 8, false);
    TYPE_BOOL   = type_create_primitive("Bool",   1, false);
    TYPE_STRING = type_create_primitive("String", 0, false);
    TYPE_VOID   = type_create_primitive("Void",   0, false);
    TYPE_UNKNOWN = type_create_primitive("<unknown>", 0, false);
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
    free(TYPE_VOID->name);   free(TYPE_VOID);
    free(TYPE_UNKNOWN->name);free(TYPE_UNKNOWN);

    TYPE_INT = TYPE_LONG = TYPE_FLOAT = TYPE_DOUBLE =
    TYPE_BOOL = TYPE_STRING = TYPE_VOID = TYPE_UNKNOWN = NULL;
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
    t->name                  = strdup(name);
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
    t->name                       = strdup(param_name);
    t->data.generic.param_name    = strdup(param_name);
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
    strcpy(t->name, constructor->name);
    strcat(t->name, "<");
    for (size_t i = 0; i < arg_count; i++) {
        strcat(t->name, args[i]->name);
        if (i + 1 < arg_count)
            strcat(t->name, ", ");
    }
    strcat(t->name, ">");

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
    strcpy(t->name, "(");
    for (size_t i = 0; i < param_count; i++) {
        strcat(t->name, params[i]->name);
        if (i + 1 < param_count)
            strcat(t->name, ", ");
    }
    strcat(t->name, ") -> ");
    strcat(t->name, return_type->name);

    t->data.function.return_type  = return_type;
    t->data.function.param_count  = param_count;
    t->data.function.param_types  = malloc(param_count * sizeof(Type *));
    if (t->data.function.param_types == NULL) {
        free(t->name);
        free(t);
        return NULL;
    }
    memcpy(t->data.function.param_types, params,
           param_count * sizeof(Type *));
    return t;
}

Type *
type_create_slot(Type *inner_type, bool is_secure)
{
    Type *t = calloc(1, sizeof(Type));
    if (t == NULL)
        return NULL;

    t->kind = TYPE_KIND_SLOT;

    const char *prefix = is_secure ? "SecureSlot<" : "Slot<";
    size_t name_len = strlen(prefix) + strlen(inner_type->name) + 2;
    t->name = malloc(name_len);
    if (t->name == NULL) {
        free(t);
        return NULL;
    }
    strcpy(t->name, prefix);
    strcat(t->name, inner_type->name);
    strcat(t->name, ">");

    t->data.slot.inner_type    = inner_type;
    t->data.slot.is_secure     = is_secure;
    t->data.slot.security_level = 0;
    return t;
}

/* -----------------------------------------------------------------
 * Type equality and compatibility
 * ----------------------------------------------------------------- */

bool
type_equals(const Type *a, const Type *b)
{
    if (a == b)
        return true;
    if (a == NULL || b == NULL)
        return false;
    if (a->kind != b->kind)
        return false;

    /* Primitive: compare by name */
    if (a->kind == TYPE_KIND_PRIMITIVE)
        return strcmp(a->name, b->name) == 0;

    /* Slot: compare inner type and security flag */
    if (a->kind == TYPE_KIND_SLOT) {
        return a->data.slot.is_secure == b->data.slot.is_secure
            && type_equals(a->data.slot.inner_type,
                           b->data.slot.inner_type);
    }

    /* Constructed: compare constructor + all args */
    if (a->kind == TYPE_KIND_CONSTRUCTED) {
        if (a->data.constructed.arg_count != b->data.constructed.arg_count)
            return false;
        if (!type_equals(a->data.constructed.constructor,
                         b->data.constructed.constructor))
            return false;
        for (size_t i = 0; i < a->data.constructed.arg_count; i++) {
            if (!type_equals(a->data.constructed.args[i],
                             b->data.constructed.args[i]))
                return false;
        }
        return true;
    }

    /* Generic: compare param names */
    if (a->kind == TYPE_KIND_GENERIC)
        return strcmp(a->data.generic.param_name,
                      b->data.generic.param_name) == 0;

    /* Function: compare params + return */
    if (a->kind == TYPE_KIND_FUNCTION) {
        if (a->data.function.param_count != b->data.function.param_count)
            return false;
        if (!type_equals(a->data.function.return_type,
                         b->data.function.return_type))
            return false;
        for (size_t i = 0; i < a->data.function.param_count; i++) {
            if (!type_equals(a->data.function.param_types[i],
                             b->data.function.param_types[i]))
                return false;
        }
        return true;
    }

    /* Fallback: name comparison */
    return strcmp(a->name, b->name) == 0;
}

bool
type_is_assignable(const Type *from, const Type *to)
{
    if (type_equals(from, to))
        return true;

    /* Int → Long widening */
    if (from->kind == TYPE_KIND_PRIMITIVE
        && to->kind == TYPE_KIND_PRIMITIVE) {
        if (strcmp(from->name, "Int") == 0
            && strcmp(to->name, "Long") == 0)
            return true;
        if (strcmp(from->name, "Float") == 0
            && strcmp(to->name, "Double") == 0)
            return true;
    }

    return false;
}

bool
type_satisfies_constraint(const Type *type, const Type *constraint)
{
    /*
     * Minimal implementation: a type satisfies a constraint if
     * it equals the constraint type.
     * Trait satisfaction will be expanded in Phase 2.
     */
    return type_equals(type, constraint);
}

/* -----------------------------------------------------------------
 * Type inference (simple expression-level)
 * Delegates to type_checker for full expression inference.
 * ----------------------------------------------------------------- */

Type *
type_infer_expression(const ASTNode *expr, TypeEnv *env)
{
    (void)expr;
    (void)env;
    /*
     * Placeholder: full inference is in type_checker.c.
     * This stub exists so existing header references compile.
     */
    return TYPE_UNKNOWN;
}

bool
type_unify(Type *a, Type *b, TypeEnv *env)
{
    (void)env;
    return type_equals(a, b);
}

/* -----------------------------------------------------------------
 * Type environment (TypeEnv wrappers — thin shim over Scope)
 * ----------------------------------------------------------------- */

TypeEnv *
type_env_create(TypeEnv *parent)
{
    TypeEnv *env = calloc(1, sizeof(TypeEnv));
    if (env == NULL)
        return NULL;
    env->parent = parent;
    return env;
}

void
type_env_destroy(TypeEnv *env)
{
    if (env == NULL)
        return;
    free(env->variables);
    free(env->types);
    free(env);
}

void
type_env_add_variable(TypeEnv *env, const char *name, Type *type)
{
    /*
     * Grow the variables array by 1.
     * (A hash map would be better for large scopes; this is sufficient
     *  for the initial implementation.)
     */
    size_t n = env->var_count;
    void *grown = realloc(env->variables,
                          (n + 1) * sizeof(*env->variables));
    if (grown == NULL)
        return;
    env->variables = grown;
    env->variables[n].name = (char *)name; /* Caller owns the string */
    env->variables[n].type = type;
    env->var_count++;
}

Type *
type_env_lookup_variable(TypeEnv *env, const char *name)
{
    TypeEnv *cur = env;
    while (cur != NULL) {
        for (size_t i = 0; i < cur->var_count; i++) {
            if (strcmp(cur->variables[i].name, name) == 0)
                return cur->variables[i].type;
        }
        cur = cur->parent;
    }
    return NULL;
}

void
type_env_add_type(TypeEnv *env, const char *name, Type *type)
{
    size_t n = env->type_count;
    void *grown = realloc(env->types,
                          (n + 1) * sizeof(*env->types));
    if (grown == NULL)
        return;
    env->types = grown;
    env->types[n].name = (char *)name;
    env->types[n].type = type;
    env->type_count++;
}

Type *
type_env_lookup_type(TypeEnv *env, const char *name)
{
    TypeEnv *cur = env;
    while (cur != NULL) {
        for (size_t i = 0; i < cur->type_count; i++) {
            if (strcmp(cur->types[i].name, name) == 0)
                return cur->types[i].type;
        }
        cur = cur->parent;
    }
    return NULL;
}

/* -----------------------------------------------------------------
 * Generic instantiation
 * ----------------------------------------------------------------- */

Type *
type_instantiate(Type *generic_type, Type **type_args, size_t arg_count)
{
    if (generic_type == NULL || arg_count == 0)
        return generic_type;

    /*
     * Minimal: if the generic type itself is a TYPE_KIND_GENERIC
     * parameter, return the first substitution.
     */
    if (generic_type->kind == TYPE_KIND_GENERIC && arg_count >= 1)
        return type_args[0];

    /*
     * Constructed type: substitute args recursively.
     * Full Hindley-Milner substitution deferred to Phase 2.
     */
    if (generic_type->kind == TYPE_KIND_CONSTRUCTED) {
        size_t n = generic_type->data.constructed.arg_count;
        Type **new_args = malloc(n * sizeof(Type *));
        if (new_args == NULL)
            return NULL;

        for (size_t i = 0; i < n; i++) {
            new_args[i] = type_instantiate(
                generic_type->data.constructed.args[i],
                type_args, arg_count);
        }
        Type *result = type_create_constructed(
            generic_type->data.constructed.constructor,
            new_args, n);
        free(new_args);
        return result;
    }

    return generic_type;
}
