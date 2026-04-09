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
    free(TYPE_DEVICE_SLOT->name); free(TYPE_DEVICE_SLOT);
    free(TYPE_ALLOCATOR->name); free(TYPE_ALLOCATOR);
    free(TYPE_RESULT->name); free(TYPE_RESULT);
    free(TYPE_OPTION->name); free(TYPE_OPTION);

    TYPE_INT = TYPE_LONG = TYPE_FLOAT = TYPE_DOUBLE =
    TYPE_BOOL = TYPE_STRING = TYPE_QUBIT = TYPE_VOID = TYPE_UNKNOWN =
    TYPE_ARRAY = TYPE_SLICE = TYPE_LIST = TYPE_QUEUE = TYPE_HASHMAP = TYPE_SET = TYPE_BOX = TYPE_RC =
        TYPE_WEAK = TYPE_CHANNEL = TYPE_FUTURE = TYPE_REMOTE_FUTURE =
    TYPE_DEVICE_SLOT = TYPE_ALLOCATOR = TYPE_RESULT = TYPE_OPTION = NULL;
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

uint32_t
type_function_effects(const Type *type)
{
    if (type == NULL || type->kind != TYPE_KIND_FUNCTION)
        return EFFECT_NONE;
    return type->data.function.effect_mask;
}

bool
type_effect_mask_has(uint32_t mask, uint32_t effect)
{
    return (mask & effect) == effect;
}

Type *
type_create_slot(Type *inner_type, bool is_secure)
{
    return type_create_slot_access(inner_type, is_secure, SLOT_ACCESS_OWNED);
}

Type *
type_create_slot_access(Type *inner_type, bool is_secure, SlotAccessMode access_mode)
{
    Type *t = calloc(1, sizeof(Type));
    if (t == NULL)
        return NULL;

    t->kind = TYPE_KIND_SLOT;

    const char *prefix = "Slot<";
    if (access_mode == SLOT_ACCESS_READ_VIEW)
        prefix = "ReadView<";
    else if (access_mode == SLOT_ACCESS_WRITE_VIEW)
        prefix = "WriteView<";
    else if (access_mode == SLOT_ACCESS_MOVE_TOKEN)
        prefix = "MoveToken<";
    else if (is_secure)
        prefix = "SecureSlot<";
    size_t name_len = strlen(prefix) + strlen(inner_type->name) + 2;
    t->name = malloc(name_len);
    if (t->name == NULL) {
        free(t);
        return NULL;
    }
    {
        size_t offset = 0;
        size_t prefix_len = strlen(prefix);
        memcpy(t->name + offset, prefix, prefix_len);
        offset += prefix_len;
        {
            size_t inner_len = strlen(inner_type->name);
            memcpy(t->name + offset, inner_type->name, inner_len);
            offset += inner_len;
        }
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
            && a->data.slot.access_mode == b->data.slot.access_mode
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

    /* Unknown types are always assignable (unresolved members, etc.) */
    if (from == TYPE_UNKNOWN || to == TYPE_UNKNOWN)
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

    /* Enum → Int implicit coercion (enums are integer-backed) */
    if (from->kind == TYPE_KIND_ENUM && to->kind == TYPE_KIND_PRIMITIVE
        && strcmp(to->name, "Int") == 0)
        return true;

    /* Bare class → constructed class (generic class constructor):
     * Pair → Pair<Int> when Pair is the constructor of Pair<Int>. */
    if (from->kind == TYPE_KIND_CLASS
        && to->kind == TYPE_KIND_CONSTRUCTED
        && to->data.constructed.constructor != NULL
        && to->data.constructed.constructor->kind == TYPE_KIND_CLASS
        && from->name != NULL && to->data.constructed.constructor->name != NULL
        && strcmp(from->name, to->data.constructed.constructor->name) == 0)
        return true;

    return false;
}

bool
type_satisfies_constraint(const Type *type, const Type *constraint)
{
    if (type == NULL || constraint == NULL)
        return false;

    /* Direct type equality */
    if (type_equals(type, constraint))
        return true;

    /* If constraint is a trait/ability name, check if type implements it */
    if (constraint->kind == TYPE_KIND_CLASS) {
        /* Future: check if type implements the trait/ability */
        return type_equals(type, constraint);
    }

    return false;
}

/* -----------------------------------------------------------------
 * Type inference (simple expression-level)
 * Delegates to type_checker for full expression inference.
 * ----------------------------------------------------------------- */

Type *
type_infer_expression(const ASTNode *expr, TypeEnv *env)
{
    if (expr == NULL)
        return TYPE_UNKNOWN;

    switch (expr->type) {
    case AST_NUMBER:
        return expr->data.number.value == (int64_t)expr->data.number.value
            ? TYPE_INT
            : TYPE_FLOAT;

    case AST_STRING:
        return TYPE_STRING;

    case AST_BOOLEAN:
        return TYPE_BOOL;

    case AST_IDENTIFIER: {
        Type *var_type = type_env_lookup_variable(env, expr->data.identifier.name);
        if (var_type != NULL)
            return var_type;

        Type *named_type = type_env_lookup_type(env, expr->data.identifier.name);
        if (named_type != NULL)
            return named_type;

        return TYPE_UNKNOWN;
    }

    case AST_TYPE: {
        Type *named_type = type_env_lookup_type(env, expr->data.type.name);
        return named_type != NULL ? named_type : TYPE_UNKNOWN;
    }

    case AST_MEMBER_ACCESS: {
        Type *object_type = type_infer_expression(expr->data.member.object, env);
        if (object_type != NULL
            && object_type->kind == TYPE_KIND_CONSTRUCTED
            && strcmp(expr->data.member.name, "Length") == 0
            && (type_equals(object_type->data.constructed.constructor, TYPE_ARRAY)
                || type_equals(object_type->data.constructed.constructor, TYPE_SLICE))) {
            return TYPE_INT;
        }
        return TYPE_UNKNOWN;
    }

    case AST_ARRAY_ACCESS: {
        Type *array_type = type_infer_expression(expr->data.array_access.array, env);
        if (array_type != NULL
            && array_type->kind == TYPE_KIND_CONSTRUCTED
            && array_type->data.constructed.arg_count >= 1) {
            return array_type->data.constructed.args[0];
        }
        return TYPE_UNKNOWN;
    }

    case AST_ASSIGNMENT:
        return type_infer_expression(expr->data.assignment.target, env);

    case AST_BINARY: {
        Type *left = type_infer_expression(expr->data.binary.left, env);
        Type *right = type_infer_expression(expr->data.binary.right, env);
        PgyTokenType op = expr->data.binary.op.type;

        switch (op) {
        case TOKEN_AND:
        case TOKEN_OR:
        case TOKEN_EQUAL:
        case TOKEN_NOT_EQUAL:
        case TOKEN_LESS:
        case TOKEN_LESS_EQUAL:
        case TOKEN_GREATER:
        case TOKEN_GREATER_EQUAL:
            return TYPE_BOOL;

        case TOKEN_PLUS:
            if (left == TYPE_STRING || right == TYPE_STRING)
                return TYPE_STRING;
            break;

        default:
            break;
        }

        if (left == TYPE_DOUBLE || right == TYPE_DOUBLE)
            return TYPE_DOUBLE;
        if (left == TYPE_FLOAT || right == TYPE_FLOAT)
            return TYPE_FLOAT;
        if (left == TYPE_LONG || right == TYPE_LONG)
            return TYPE_LONG;
        if (left != NULL && left != TYPE_UNKNOWN)
            return left;
        if (right != NULL && right != TYPE_UNKNOWN)
            return right;
        return TYPE_UNKNOWN;
    }

    case AST_UNARY: {
        Type *operand = type_infer_expression(expr->data.unary.operand, env);
        if (expr->data.unary.op.type == TOKEN_NOT)
            return TYPE_BOOL;
        return operand != NULL ? operand : TYPE_UNKNOWN;
    }

    case AST_CALL:
        if (expr->data.call.callee != NULL
            && expr->data.call.callee->type == AST_IDENTIFIER) {
            const char *callee = expr->data.call.callee->data.identifier.name;

            if (strcmp(callee, "Read") == 0 && expr->data.call.arg_count >= 1) {
                Type *slot_type = type_infer_expression(expr->data.call.arguments[0], env);
                if (slot_type != NULL && slot_type->kind == TYPE_KIND_SLOT)
                    return slot_type->data.slot.inner_type;
            }
            if ((strcmp(callee, "ClaimSlot") == 0 || strcmp(callee, "ClaimSecureSlot") == 0)
                && expr->data.call.arg_count >= 1) {
                Type *inner = type_infer_expression(expr->data.call.arguments[0], env);
                if (inner != NULL && inner != TYPE_UNKNOWN)
                    return type_create_slot(inner, strcmp(callee, "ClaimSecureSlot") == 0);
            }
            if (strcmp(callee, "RcClone") == 0 && expr->data.call.arg_count >= 1)
                return type_infer_expression(expr->data.call.arguments[0], env);
            if (strcmp(callee, "RcDowngrade") == 0 && expr->data.call.arg_count >= 1) {
                Type *rc_type = type_infer_expression(expr->data.call.arguments[0], env);
                if (rc_type != NULL
                    && rc_type->kind == TYPE_KIND_CONSTRUCTED
                    && type_equals(rc_type->data.constructed.constructor, TYPE_RC)
                    && rc_type->data.constructed.arg_count == 1) {
                    return type_create_constructed(TYPE_WEAK,
                        rc_type->data.constructed.args,
                        rc_type->data.constructed.arg_count);
                }
            }
            if (strcmp(callee, "WeakUpgrade") == 0 && expr->data.call.arg_count >= 1) {
                Type *weak_type = type_infer_expression(expr->data.call.arguments[0], env);
                if (weak_type != NULL
                    && weak_type->kind == TYPE_KIND_CONSTRUCTED
                    && type_equals(weak_type->data.constructed.constructor, TYPE_WEAK)
                    && weak_type->data.constructed.arg_count == 1) {
                    return type_create_constructed(TYPE_RC,
                        weak_type->data.constructed.args,
                        weak_type->data.constructed.arg_count);
                }
            }
            if (strcmp(callee, "AllocatorSystem") == 0
                || strcmp(callee, "AllocatorTracing") == 0
                || strcmp(callee, "AllocatorDebug") == 0
                || strcmp(callee, "AllocatorPool") == 0) {
                return TYPE_ALLOCATOR;
            }
            if (strcmp(callee, "ClaimQubit") == 0)
                return TYPE_QUBIT;
            if (strcmp(callee, "Measure") == 0
                || strcmp(callee, "QubitState") == 0)
                return TYPE_INT;
            if (strcmp(callee, "IsCollapsed") == 0
                || strcmp(callee, "IntoClassical") == 0)
                return TYPE_BOOL;
        }

        return TYPE_UNKNOWN;

    case AST_AWAIT_EXPR: {
        Type *inner = type_infer_expression(expr->data.await_expr.expression, env);
        if (inner != NULL
            && inner->kind == TYPE_KIND_CONSTRUCTED
            && inner->data.constructed.arg_count == 1) {
            return inner->data.constructed.args[0];
        }
        return TYPE_UNKNOWN;
    }

    case AST_SPAWN_EXPR: {
        Type *inner = type_infer_expression(expr->data.spawn_expr.function, env);
        Type *args[1] = { inner != NULL ? inner : TYPE_UNKNOWN };
        return type_create_constructed(TYPE_FUTURE, args, 1);
    }

    case AST_CHANNEL_RECV: {
        Type *channel_type = type_infer_expression(expr->data.channel_recv.channel, env);
        if (channel_type != NULL
            && channel_type->kind == TYPE_KIND_CONSTRUCTED
            && channel_type->data.constructed.arg_count == 1) {
            return channel_type->data.constructed.args[0];
        }
        return TYPE_UNKNOWN;
    }

    case AST_CHANNEL_SEND:
    case AST_EVENT_INVOKE:
        return TYPE_VOID;

    case AST_LAMBDA_EXPR: {
        size_t param_count = expr->data.lambda_expr.param_count;
        Type **params = calloc(param_count == 0 ? 1 : param_count, sizeof(Type *));
        if (params == NULL)
            return TYPE_UNKNOWN;
        for (size_t i = 0; i < param_count; i++) {
            params[i] = TYPE_UNKNOWN;
        }
        Type *return_type = expr->data.lambda_expr.return_type != NULL
            ? type_infer_expression(expr->data.lambda_expr.return_type, env)
            : TYPE_UNKNOWN;
        Type *fn_type = type_create_function(params, param_count, return_type);
        free(params);
        return fn_type != NULL ? fn_type : TYPE_UNKNOWN;
    }

    default:
        return TYPE_UNKNOWN;
    }
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
