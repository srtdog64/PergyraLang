/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker implementation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "../common/string_compat.h"
#include "type_checker.h"

#define INITIAL_DIAG_CAPACITY 16

/* -----------------------------------------------------------------
 * Context lifecycle
 * ----------------------------------------------------------------- */

SemanticContext *
semantic_context_create(void)
{
    SemanticContext *ctx = calloc(1, sizeof(SemanticContext));
    if (ctx == NULL)
        return NULL;

    ctx->scope               = scope_create(NULL, SCOPE_GLOBAL);
    ctx->diagnostic_capacity = INITIAL_DIAG_CAPACITY;
    ctx->diagnostics         = calloc(INITIAL_DIAG_CAPACITY,
                                      sizeof(Diagnostic *));
    if (ctx->scope == NULL || ctx->diagnostics == NULL) {
        scope_destroy(ctx->scope);
        free(ctx->diagnostics);
        free(ctx);
        return NULL;
    }

    /* Register built-in types in global scope */
    type_system_init();

    return ctx;
}

void
semantic_context_destroy(SemanticContext *ctx)
{
    if (ctx == NULL)
        return;

    scope_destroy(ctx->scope);

    for (size_t i = 0; i < ctx->diagnostic_count; i++) {
        free(ctx->diagnostics[i]->message);
        free(ctx->diagnostics[i]);
    }
    free(ctx->diagnostics);
    free(ctx);
}

/* -----------------------------------------------------------------
 * Diagnostic emission
 * ----------------------------------------------------------------- */

static void
emit_diagnostic(SemanticContext *ctx, DiagnosticLevel level,
                 const ASTNode *node, const char *fmt, va_list ap)
{
    if (ctx->diagnostic_count >= ctx->diagnostic_capacity) {
        size_t new_cap = ctx->diagnostic_capacity * 2;
        Diagnostic **grown = realloc(ctx->diagnostics,
                                     new_cap * sizeof(Diagnostic *));
        if (grown == NULL)
            return;
        ctx->diagnostics         = grown;
        ctx->diagnostic_capacity = new_cap;
    }

    Diagnostic *d = calloc(1, sizeof(Diagnostic));
    if (d == NULL)
        return;

    d->level = level;
    d->line  = node ? node->line   : 0;
    d->col   = node ? node->column : 0;

    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    d->message = pergyra_strdup(buf);

    ctx->diagnostics[ctx->diagnostic_count++] = d;

    if (level == DIAG_ERROR)
        ctx->has_error = true;
}

void
semantic_error(SemanticContext *ctx, const ASTNode *node,
               const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit_diagnostic(ctx, DIAG_ERROR, node, fmt, ap);
    va_end(ap);
}

void
semantic_warning(SemanticContext *ctx, const ASTNode *node,
                  const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit_diagnostic(ctx, DIAG_WARNING, node, fmt, ap);
    va_end(ap);
}

void
semantic_print_diagnostics(SemanticContext *ctx)
{
    for (size_t i = 0; i < ctx->diagnostic_count; i++) {
        Diagnostic *d = ctx->diagnostics[i];
        const char *level = (d->level == DIAG_ERROR) ? "ERROR" : "WARNING";
        fprintf(stderr, "[%s] %u:%u - %s\n",
                level, d->line, d->col, d->message);
    }
}

/* -----------------------------------------------------------------
 * Utility — resolve AST type node to Type*
 * ----------------------------------------------------------------- */

static Type *
resolve_named_type(const char *name, SemanticContext *ctx, const ASTNode *site)
{
    if (strcmp(name, "Int")    == 0) return TYPE_INT;
    if (strcmp(name, "Long")   == 0) return TYPE_LONG;
    if (strcmp(name, "Float")  == 0) return TYPE_FLOAT;
    if (strcmp(name, "Double") == 0) return TYPE_DOUBLE;
    if (strcmp(name, "Bool")   == 0) return TYPE_BOOL;
    if (strcmp(name, "String") == 0) return TYPE_STRING;
    if (strcmp(name, "Void")   == 0) return TYPE_VOID;
    if (strcmp(name, "Array")  == 0) return TYPE_ARRAY;
    if (strcmp(name, "Slice")  == 0) return TYPE_SLICE;
    if (strcmp(name, "Box")    == 0) return TYPE_BOX;
    if (strcmp(name, "Rc")     == 0) return TYPE_RC;
    if (strcmp(name, "Weak")   == 0) return TYPE_WEAK;
    if (strcmp(name, "Allocator") == 0) return TYPE_ALLOCATOR;

    Symbol *sym = scope_lookup(ctx->scope, name);
    if (sym != NULL)
        return sym->type;

    semantic_error(ctx, site, "Unknown type '%s'", name);
    return TYPE_UNKNOWN;
}

static Type *
resolve_generic_type_arg(GenericParam *gp, SemanticContext *ctx,
                         const ASTNode *site)
{
    if (gp == NULL)
        return TYPE_UNKNOWN;
    if (gp->constraint != NULL)
        return resolve_type_node(gp->constraint, ctx);
    return resolve_named_type(gp->name, ctx, site);
}

static bool
type_is_constructed_named(const Type *type, const char *name)
{
    return type != NULL
        && type->kind == TYPE_KIND_CONSTRUCTED
        && type->data.constructed.constructor != NULL
        && strcmp(type->data.constructed.constructor->name, name) == 0;
}

static Type *
type_get_constructed_arg(const Type *type, size_t index)
{
    if (type == NULL || type->kind != TYPE_KIND_CONSTRUCTED)
        return TYPE_UNKNOWN;
    if (index >= type->data.constructed.arg_count)
        return TYPE_UNKNOWN;
    return type->data.constructed.args[index];
}

Type *
resolve_type_node(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL)
        return TYPE_VOID;

    if (node->type == AST_CHANNEL_TYPE) {
        Type *inner = resolve_type_node(node->data.channel_type.element_type, ctx);
        Type *args[1] = { inner };
        return type_create_constructed(TYPE_CHANNEL, args, 1);
    }

    if (node->type == AST_FUTURE_TYPE) {
        Type *inner = resolve_type_node(node->data.future_type.value_type, ctx);
        Type *args[1] = { inner };
        return type_create_constructed(TYPE_FUTURE, args, 1);
    }

    if (node->type != AST_TYPE)
        return TYPE_UNKNOWN;

    const char *name = node->data.type.name;

    if (strcmp(name, "Slot") == 0 || strcmp(name, "SecureSlot") == 0) {
        if (node->data.type.generic_args == NULL
            || node->data.type.generic_args->count != 1) {
            semantic_error(ctx, node,
                "%s requires exactly one type argument", name);
            return TYPE_UNKNOWN;
        }
        Type *inner = resolve_generic_type_arg(
            node->data.type.generic_args->params[0], ctx, node);
        return type_create_slot(inner, strcmp(name, "SecureSlot") == 0);
    }

    if (strcmp(name, "Array") == 0 || strcmp(name, "Slice") == 0
        || strcmp(name, "Box") == 0 || strcmp(name, "Rc") == 0
        || strcmp(name, "Weak") == 0 || strcmp(name, "Channel") == 0
        || strcmp(name, "Future") == 0 || strcmp(name, "Result") == 0) {
        if (node->data.type.generic_args == NULL
            || node->data.type.generic_args->count != 1) {
            semantic_error(ctx, node,
                "%s requires exactly one type argument", name);
            return TYPE_UNKNOWN;
        }

        Type *inner = resolve_generic_type_arg(
            node->data.type.generic_args->params[0], ctx, node);
        Type *constructor = TYPE_UNKNOWN;
        if (strcmp(name, "Array") == 0) constructor = TYPE_ARRAY;
        else if (strcmp(name, "Slice") == 0) constructor = TYPE_SLICE;
        else if (strcmp(name, "Box") == 0) constructor = TYPE_BOX;
        else if (strcmp(name, "Rc") == 0) constructor = TYPE_RC;
        else if (strcmp(name, "Weak") == 0) constructor = TYPE_WEAK;
        else if (strcmp(name, "Channel") == 0) constructor = TYPE_CHANNEL;
        else if (strcmp(name, "Future") == 0) constructor = TYPE_FUTURE;
        else if (strcmp(name, "Result") == 0) constructor = TYPE_UNKNOWN;
        Type *args[1] = { inner };
        return type_create_constructed(constructor, args, 1);
    }

    return resolve_named_type(name, ctx, node);
}

bool
require_assignable(Type *from, Type *to,
                    const ASTNode *site, SemanticContext *ctx)
{
    if (type_is_assignable(from, to))
        return true;

    semantic_error(ctx, site,
        "Type mismatch: cannot assign '%s' to '%s'",
        from->name, to->name);
    return false;
}

/* -----------------------------------------------------------------
 * Built-in dispatch
 * ----------------------------------------------------------------- */

BuiltinKind
builtin_resolve(const char *name)
{
    if (strcmp(name, "ClaimSlot")       == 0) return BUILTIN_CLAIM_SLOT;
    if (strcmp(name, "ClaimSecureSlot") == 0) return BUILTIN_CLAIM_SECURE_SLOT;
    if (strcmp(name, "Write")           == 0) return BUILTIN_WRITE;
    if (strcmp(name, "Read")            == 0) return BUILTIN_READ;
    if (strcmp(name, "Release")         == 0) return BUILTIN_RELEASE;
    if (strcmp(name, "Log")             == 0) return BUILTIN_LOG;
    if (strcmp(name, "RcNew")           == 0) return BUILTIN_RC_NEW;
    if (strcmp(name, "RcClone")         == 0) return BUILTIN_RC_CLONE;
    if (strcmp(name, "RcDrop")          == 0) return BUILTIN_RC_DROP;
    if (strcmp(name, "RcDowngrade")     == 0) return BUILTIN_RC_DOWNGRADE;
    if (strcmp(name, "RcGet")           == 0) return BUILTIN_RC_GET;
    if (strcmp(name, "WeakUpgrade")     == 0) return BUILTIN_WEAK_UPGRADE;
    if (strcmp(name, "WeakDrop")        == 0) return BUILTIN_WEAK_DROP;
    if (strcmp(name, "AllocatorSystem") == 0) return BUILTIN_ALLOCATOR_SYSTEM;
    if (strcmp(name, "AllocatorTracing")== 0) return BUILTIN_ALLOCATOR_TRACING;
    if (strcmp(name, "AllocatorDebug")  == 0) return BUILTIN_ALLOCATOR_DEBUG;
    if (strcmp(name, "AllocatorPool")   == 0) return BUILTIN_ALLOCATOR_POOL;
    if (strcmp(name, "Box")             == 0) return BUILTIN_BOX;
    if (strcmp(name, "BoxArray")        == 0) return BUILTIN_BOX_ARRAY;
    return BUILTIN_NOT_BUILTIN;
}

/* -----------------------------------------------------------------
 * Slot built-in checkers
 * ----------------------------------------------------------------- */

Type *
type_check_claim_slot(ASTNode *call, SemanticContext *ctx)
{
    /*
     * ClaimSlot<T>() → registers a Slot<T> symbol when used in let decl.
     * Here we just return the type; symbol registration happens in
     * type_check_let_decl.
     */
    if (call->data.call.arg_count != 0) {
        semantic_error(ctx, call, "ClaimSlot takes no arguments");
        return TYPE_UNKNOWN;
    }

    /* Generic args come through the callee's type annotation */
    ASTNode *callee = call->data.call.callee;
    if (callee->type == AST_IDENTIFIER
        && callee->data.identifier.name != NULL) {
        /* Type arg must be resolved from the let-decl annotation */
        return TYPE_UNKNOWN; /* Resolved in type_check_let_decl */
    }

    return TYPE_UNKNOWN;
}

bool
type_check_write_slot(ASTNode *call, SemanticContext *ctx)
{
    size_t arg_count = call->data.call.arg_count;

    if (arg_count < 2) {
        semantic_error(ctx, call,
            "Write requires at least 2 arguments: Write(slot, value)");
        return false;
    }

    /* Resolve slot argument */
    ASTNode *slot_arg = call->data.call.arguments[0];
    Type    *slot_type = type_check_expression(slot_arg, ctx);

    if (slot_type->kind != TYPE_KIND_SLOT) {
        semantic_error(ctx, slot_arg,
            "First argument to Write must be a Slot, got '%s'",
            slot_type->name);
        return false;
    }

    /* Rule R4: slot must be CLAIMED */
    if (slot_arg->type == AST_IDENTIFIER) {
        Symbol *sym = scope_lookup(ctx->scope,
                                    slot_arg->data.identifier.name);
        if (sym != NULL && sym->kind == SYMBOL_SLOT) {
            if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error(ctx, slot_arg,
                    "Cannot write to released slot '%s'",
                    sym->name);
                return false;
            }

            /* Rule R2: SecureSlot requires token */
            if (sym->slot_info.is_secure) {
                if (arg_count < 3) {
                    semantic_error(ctx, call,
                        "Write to SecureSlot '%s' requires a token argument",
                        sym->name);
                    return false;
                }

                /* Rule R3: token must be paired with this slot */
                ASTNode *token_arg = call->data.call.arguments[2];
                if (token_arg->type == AST_IDENTIFIER) {
                    const char *token_name =
                        token_arg->data.identifier.name;
                    if (sym->slot_info.paired_token_name == NULL
                        || strcmp(sym->slot_info.paired_token_name,
                                  token_name) != 0) {
                        semantic_error(ctx, token_arg,
                            "Token '%s' is not paired with slot '%s'",
                            token_name, sym->name);
                        return false;
                    }
                }
            } else if (arg_count > 2) {
                semantic_warning(ctx, call,
                    "Write to plain Slot '%s' ignores extra token argument",
                    sym->name);
            }
        }
    }

    /* Rule R1: value type must match Slot inner type */
    ASTNode *value_arg  = call->data.call.arguments[1];
    Type    *value_type = type_check_expression(value_arg, ctx);
    Type    *inner_type = slot_type->data.slot.inner_type;

    if (!type_is_assignable(value_type, inner_type)) {
        semantic_error(ctx, value_arg,
            "Cannot write '%s' to %s (expected '%s')",
            value_type->name, slot_type->name, inner_type->name);
        return false;
    }

    return true;
}

Type *
type_check_read_slot(ASTNode *call, SemanticContext *ctx)
{
    size_t arg_count = call->data.call.arg_count;

    if (arg_count < 1) {
        semantic_error(ctx, call,
            "Read requires at least 1 argument: Read(slot)");
        return TYPE_UNKNOWN;
    }

    ASTNode *slot_arg  = call->data.call.arguments[0];
    Type    *slot_type = type_check_expression(slot_arg, ctx);

    if (slot_type->kind != TYPE_KIND_SLOT) {
        semantic_error(ctx, slot_arg,
            "First argument to Read must be a Slot, got '%s'",
            slot_type->name);
        return TYPE_UNKNOWN;
    }

    if (slot_arg->type == AST_IDENTIFIER) {
        Symbol *sym = scope_lookup(ctx->scope,
                                    slot_arg->data.identifier.name);
        if (sym != NULL && sym->kind == SYMBOL_SLOT) {
            if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error(ctx, slot_arg,
                    "Cannot read from released slot '%s'", sym->name);
                return TYPE_UNKNOWN;
            }

            if (sym->slot_info.is_secure) {
                if (arg_count < 2) {
                    semantic_error(ctx, call,
                        "Read from SecureSlot '%s' requires a token argument",
                        sym->name);
                    return TYPE_UNKNOWN;
                }
                ASTNode *token_arg = call->data.call.arguments[1];
                if (token_arg->type == AST_IDENTIFIER) {
                    const char *token_name =
                        token_arg->data.identifier.name;
                    if (sym->slot_info.paired_token_name == NULL
                        || strcmp(sym->slot_info.paired_token_name,
                                  token_name) != 0) {
                        semantic_error(ctx, token_arg,
                            "Token '%s' is not paired with slot '%s'",
                            token_name, sym->name);
                        return TYPE_UNKNOWN;
                    }
                }
            }
        }
    }

    return slot_type->data.slot.inner_type;
}

bool
type_check_release_slot(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count < 1) {
        semantic_error(ctx, call,
            "Release requires at least 1 argument: Release(slot)");
        return false;
    }

    ASTNode *slot_arg = call->data.call.arguments[0];

    if (slot_arg->type != AST_IDENTIFIER) {
        semantic_error(ctx, slot_arg,
            "Argument to Release must be a slot identifier");
        return false;
    }

    const char *slot_name = slot_arg->data.identifier.name;
    Symbol     *sym       = scope_lookup(ctx->scope, slot_name);

    if (sym == NULL || sym->kind != SYMBOL_SLOT) {
        semantic_error(ctx, slot_arg,
            "'%s' is not a slot", slot_name);
        return false;
    }

    if (sym->slot_info.state == SLOT_STATE_RELEASED) {
        semantic_error(ctx, slot_arg,
            "Slot '%s' has already been released", slot_name);
        return false;
    }

    if (sym->slot_info.is_secure
        && call->data.call.arg_count < 2) {
        semantic_error(ctx, call,
            "Release of SecureSlot '%s' requires a token argument",
            slot_name);
        return false;
    }

    scope_release_slot(ctx->scope, slot_name);
    return true;
}

static Type *
wrap_constructed(Type *constructor, Type *inner)
{
    Type *args[1] = { inner };
    return type_create_constructed(constructor, args, 1);
}

static Type *
type_check_rc_new(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "RcNew requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *inner = type_check_expression(call->data.call.arguments[0], ctx);
    return wrap_constructed(TYPE_RC, inner);
}

static Type *
type_check_rc_clone(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "RcClone requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *rc_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(rc_type, "Rc")) {
        semantic_error(ctx, call, "RcClone requires Rc<T>, got '%s'", rc_type->name);
        return TYPE_UNKNOWN;
    }
    return rc_type;
}

static Type *
type_check_rc_get(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "RcGet requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *rc_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(rc_type, "Rc")) {
        semantic_error(ctx, call, "RcGet requires Rc<T>, got '%s'", rc_type->name);
        return TYPE_UNKNOWN;
    }
    return type_get_constructed_arg(rc_type, 0);
}

static Type *
type_check_rc_downgrade(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "RcDowngrade requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *rc_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(rc_type, "Rc")) {
        semantic_error(ctx, call, "RcDowngrade requires Rc<T>, got '%s'", rc_type->name);
        return TYPE_UNKNOWN;
    }
    return wrap_constructed(TYPE_WEAK, type_get_constructed_arg(rc_type, 0));
}

static Type *
type_check_weak_upgrade(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "WeakUpgrade requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *weak_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(weak_type, "Weak")) {
        semantic_error(ctx, call, "WeakUpgrade requires Weak<T>, got '%s'", weak_type->name);
        return TYPE_UNKNOWN;
    }
    return wrap_constructed(TYPE_RC, type_get_constructed_arg(weak_type, 0));
}

static Type *
type_check_weak_drop(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "WeakDrop requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *weak_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(weak_type, "Weak")) {
        semantic_error(ctx, call, "WeakDrop requires Weak<T>, got '%s'", weak_type->name);
        return TYPE_UNKNOWN;
    }
    return TYPE_VOID;
}

static Type *
type_check_allocator_builtin(ASTNode *call, SemanticContext *ctx, bool requires_capacity)
{
    if ((!requires_capacity && call->data.call.arg_count != 0)
        || (requires_capacity && call->data.call.arg_count != 1)) {
        semantic_error(ctx, call,
            requires_capacity
                ? "AllocatorPool requires exactly 1 capacity argument"
                : "Allocator constructor takes no arguments");
        return TYPE_UNKNOWN;
    }

    if (requires_capacity) {
        Type *cap_type = type_check_expression(call->data.call.arguments[0], ctx);
        if (!type_equals(cap_type, TYPE_INT) && !type_equals(cap_type, TYPE_LONG)) {
            semantic_error(ctx, call->data.call.arguments[0],
                "AllocatorPool capacity must be Int or Long, got '%s'",
                cap_type->name);
            return TYPE_UNKNOWN;
        }
    }

    return TYPE_ALLOCATOR;
}

static Type *
type_check_box_builtin(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "Box requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *inner = type_check_expression(call->data.call.arguments[0], ctx);
    return wrap_constructed(TYPE_BOX, inner);
}

static Type *
type_check_box_array_builtin(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count < 1 || call->data.call.arg_count > 2) {
        semantic_error(ctx, call,
            "BoxArray requires capacity and optional allocator");
        return TYPE_UNKNOWN;
    }

    Type *cap_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_equals(cap_type, TYPE_INT) && !type_equals(cap_type, TYPE_LONG)) {
        semantic_error(ctx, call->data.call.arguments[0],
            "BoxArray capacity must be Int or Long, got '%s'", cap_type->name);
        return TYPE_UNKNOWN;
    }

    if (call->data.call.arg_count == 2) {
        Type *alloc_type = type_check_expression(call->data.call.arguments[1], ctx);
        if (!type_equals(alloc_type, TYPE_ALLOCATOR)) {
            semantic_error(ctx, call->data.call.arguments[1],
                "BoxArray allocator must be Allocator, got '%s'", alloc_type->name);
            return TYPE_UNKNOWN;
        }
    }

    return TYPE_UNKNOWN;
}

/* -----------------------------------------------------------------
 * Built-in call dispatcher
 * ----------------------------------------------------------------- */

Type *
type_check_builtin_call(ASTNode *call, BuiltinKind kind,
                         SemanticContext *ctx)
{
    switch (kind) {
    case BUILTIN_CLAIM_SLOT:
        return type_check_claim_slot(call, ctx);

    case BUILTIN_WRITE:
        type_check_write_slot(call, ctx);
        return TYPE_VOID;

    case BUILTIN_READ:
        return type_check_read_slot(call, ctx);

    case BUILTIN_RELEASE:
        type_check_release_slot(call, ctx);
        return TYPE_VOID;

    case BUILTIN_LOG:
        /* Log accepts any argument count and types */
        return TYPE_VOID;

    case BUILTIN_RC_NEW:
        return type_check_rc_new(call, ctx);

    case BUILTIN_RC_CLONE:
        return type_check_rc_clone(call, ctx);

    case BUILTIN_RC_DROP:
        (void)type_check_rc_clone(call, ctx);
        return TYPE_VOID;

    case BUILTIN_RC_DOWNGRADE:
        return type_check_rc_downgrade(call, ctx);

    case BUILTIN_RC_GET:
        return type_check_rc_get(call, ctx);

    case BUILTIN_WEAK_UPGRADE:
        return type_check_weak_upgrade(call, ctx);

    case BUILTIN_WEAK_DROP:
        return type_check_weak_drop(call, ctx);

    case BUILTIN_ALLOCATOR_SYSTEM:
    case BUILTIN_ALLOCATOR_TRACING:
    case BUILTIN_ALLOCATOR_DEBUG:
        return type_check_allocator_builtin(call, ctx, false);

    case BUILTIN_ALLOCATOR_POOL:
        return type_check_allocator_builtin(call, ctx, true);

    case BUILTIN_BOX:
        return type_check_box_builtin(call, ctx);

    case BUILTIN_BOX_ARRAY:
        return type_check_box_array_builtin(call, ctx);

    case BUILTIN_PARALLEL:
        /* Handled separately by type_check_parallel_block */
        return TYPE_VOID;

    default:
        return TYPE_UNKNOWN;
    }
}

/* -----------------------------------------------------------------
 * Expression type checker
 * ----------------------------------------------------------------- */

Type *
type_check_expression(ASTNode *expr, SemanticContext *ctx)
{
    if (expr == NULL)
        return TYPE_VOID;

    switch (expr->type) {
    case AST_NUMBER:
        return TYPE_INT;

    case AST_STRING:
        return TYPE_STRING;

    case AST_BOOLEAN:
        return TYPE_BOOL;

    case AST_IDENTIFIER: {
        Symbol *sym = scope_lookup(ctx->scope,
                                    expr->data.identifier.name);
        if (sym == NULL) {
            semantic_error(ctx, expr,
                "Undefined symbol '%s'",
                expr->data.identifier.name);
            return TYPE_UNKNOWN;
        }
        sym->is_used = true;
        return sym->type;
    }

    case AST_BINARY:
        return type_check_binary(expr, ctx);

    case AST_UNARY:
        return type_check_unary(expr, ctx);

    case AST_CALL:
        return type_check_call(expr, ctx);

    case AST_MEMBER_ACCESS:
        return type_check_member_access(expr, ctx);

    case AST_ARRAY_ACCESS:
        return type_check_array_access(expr, ctx);

    case AST_ASSIGNMENT:
        return type_check_assignment(expr, ctx);

    case AST_AWAIT_EXPR:
        if (!ctx->in_async_func) {
            semantic_error(ctx, expr,
                "'await' used outside of async function");
        }
        {
            Type *future_type = type_check_expression(expr->data.await_expr.expression, ctx);
            if (future_type != NULL
                && future_type->kind == TYPE_KIND_CONSTRUCTED
                && type_equals(future_type->data.constructed.constructor, TYPE_FUTURE)
                && future_type->data.constructed.arg_count == 1) {
                return future_type->data.constructed.args[0];
            }
            semantic_error(ctx, expr->data.await_expr.expression,
                "'await' requires Future<T>");
            return TYPE_UNKNOWN;
        }

    case AST_SPAWN_EXPR:
        return type_check_spawn_expr(expr, ctx);

    case AST_CHANNEL_SEND:
        return type_check_channel_send(expr, ctx);

    case AST_CHANNEL_RECV:
        return type_check_channel_recv(expr, ctx);

    default:
        return TYPE_UNKNOWN;
    }
}

Type *
type_check_binary(ASTNode *expr, SemanticContext *ctx)
{
    Type *left  = type_check_expression(expr->data.binary.left,  ctx);
    Type *right = type_check_expression(expr->data.binary.right, ctx);

    /* If either operand is unknown, skip checks and propagate */
    if (left == TYPE_UNKNOWN || right == TYPE_UNKNOWN) {
        TokenType op = expr->data.binary.op.type;
        if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL
            || op == TOKEN_LESS     || op == TOKEN_LESS_EQUAL
            || op == TOKEN_GREATER  || op == TOKEN_GREATER_EQUAL)
            return TYPE_BOOL;
        return (left != TYPE_UNKNOWN) ? left : right;
    }

    /* Comparison operators → Bool */
    TokenType op = expr->data.binary.op.type;
    if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL
        || op == TOKEN_LESS     || op == TOKEN_LESS_EQUAL
        || op == TOKEN_GREATER  || op == TOKEN_GREATER_EQUAL) {
        if (!type_equals(left, right)) {
            semantic_error(ctx, expr,
                "Cannot compare '%s' and '%s'",
                left->name, right->name);
        }
        return TYPE_BOOL;
    }

    /* Arithmetic: both operands must match */
    if (!type_equals(left, right)) {
        semantic_error(ctx, expr,
            "Type mismatch in binary operation: '%s' and '%s'",
            left->name, right->name);
        return TYPE_UNKNOWN;
    }

    return left;
}

Type *
type_check_unary(ASTNode *expr, SemanticContext *ctx)
{
    Type *operand = type_check_expression(expr->data.unary.operand, ctx);

    TokenType op = expr->data.unary.op.type;
    if (op == TOKEN_NOT) {
        if (!type_equals(operand, TYPE_BOOL)) {
            semantic_error(ctx, expr,
                "'!' operator requires Bool, got '%s'", operand->name);
        }
        return TYPE_BOOL;
    }

    if (op == TOKEN_MINUS) {
        if (!type_equals(operand, TYPE_INT)
            && !type_equals(operand, TYPE_FLOAT)) {
            semantic_error(ctx, expr,
                "Unary '-' requires numeric type, got '%s'",
                operand->name);
        }
        return operand;
    }

    return operand;
}

Type *
type_check_call(ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *callee = expr->data.call.callee;

    if (callee->type == AST_IDENTIFIER) {
        const char *name = callee->data.identifier.name;
        BuiltinKind bk   = builtin_resolve(name);
        if (bk != BUILTIN_NOT_BUILTIN)
            return type_check_builtin_call(expr, bk, ctx);

        /* Channel(capacity) is a built-in constructor that the transpiler
         * handles; let it pass without a symbol table entry. The actual
         * type is resolved from the annotation in emit_let_decl. */
        if (strcmp(name, "Channel") == 0)
            return TYPE_UNKNOWN;  /* type inferred from let annotation */

        /* Result built-in functions */
        if (strcmp(name, "Ok") == 0 || strcmp(name, "Err") == 0
            || strcmp(name, "IsOk") == 0 || strcmp(name, "IsErr") == 0
            || strcmp(name, "Unwrap") == 0 || strcmp(name, "UnwrapOr") == 0)
            return TYPE_UNKNOWN;

        /* Standard library built-in functions */
        if (strcmp(name, "Abs") == 0 || strcmp(name, "Min") == 0
            || strcmp(name, "Max") == 0 || strcmp(name, "StringLength") == 0
            || strcmp(name, "Print") == 0 || strcmp(name, "ToString") == 0)
            return TYPE_UNKNOWN;

        Symbol *sym = scope_lookup(ctx->scope, name);
        if (sym == NULL) {
            semantic_error(ctx, expr,
                "Undefined function '%s'", name);
            return TYPE_UNKNOWN;
        }
        /* Allow class constructors: ClassName() */
        if (sym->kind == SYMBOL_CLASS) {
            sym->is_used = true;
            return sym->type;
        }

        if (sym->type->kind != TYPE_KIND_FUNCTION) {
            semantic_error(ctx, expr,
                "'%s' is not a function", name);
            return TYPE_UNKNOWN;
        }
        sym->is_used = true;

        /* Argument count check */
        size_t expected = sym->type->data.function.param_count;
        size_t provided  = expr->data.call.arg_count;
        if (provided != expected) {
            semantic_error(ctx, expr,
                "'%s' expects %zu argument(s), got %zu",
                name, expected, provided);
            return sym->type->data.function.return_type;
        }

        /* Argument type check */
        for (size_t i = 0; i < provided; i++) {
            Type *arg_type = type_check_expression(
                expr->data.call.arguments[i], ctx);
            Type *param_type =
                sym->type->data.function.param_types[i];
            require_assignable(arg_type, param_type,
                               expr->data.call.arguments[i], ctx);
        }

        return sym->type->data.function.return_type;
    }

    /* Callee is a member access (method call) */
    if (callee->type == AST_MEMBER_ACCESS) {
        /* Resolve object type; for now return UNKNOWN for method calls */
        type_check_expression(callee->data.member.object, ctx);
        return TYPE_UNKNOWN;
    }

    return TYPE_UNKNOWN;
}

Type *
type_check_member_access(ASTNode *expr, SemanticContext *ctx)
{
    Type *object_type = type_check_expression(expr->data.member.object, ctx);

    if ((type_is_constructed_named(object_type, "Array")
         || type_is_constructed_named(object_type, "Slice"))
        && strcmp(expr->data.member.name, "Length") == 0) {
        return TYPE_INT;
    }

    /* Resolve class field types */
    if (object_type != NULL && object_type->kind == TYPE_KIND_CLASS
        && object_type->name != NULL) {
        /* Look up the class declaration in scope */
        Symbol *cls_sym = scope_lookup(ctx->scope, object_type->name);
        if (cls_sym != NULL && cls_sym->kind == SYMBOL_CLASS) {
            /* For now, return a generic type — full field resolution
             * would require storing field info in the Type structure.
             * Accept any field access on class types. */
            return TYPE_UNKNOWN;
        }
    }

    /* Unknown member access — allow without error for class types */
    if (object_type != NULL && object_type->kind == TYPE_KIND_CLASS)
        return TYPE_UNKNOWN;

    return TYPE_UNKNOWN;
}

Type *
type_check_array_access(ASTNode *expr, SemanticContext *ctx)
{
    Type *object_type = type_check_expression(expr->data.array_access.array, ctx);
    Type *index_type  = type_check_expression(expr->data.array_access.index, ctx);

    if (!type_equals(index_type, TYPE_INT)) {
        semantic_error(ctx, expr->data.array_access.index,
            "Array index must be Int, got '%s'", index_type->name);
        return TYPE_UNKNOWN;
    }

    if (type_is_constructed_named(object_type, "Array")
        || type_is_constructed_named(object_type, "Slice")) {
        return type_get_constructed_arg(object_type, 0);
    }

    semantic_error(ctx, expr->data.array_access.array,
        "Index access requires Array<T> or Slice<T>, got '%s'",
        object_type->name);
    return TYPE_UNKNOWN;
}

Type *
type_check_assignment(ASTNode *expr, SemanticContext *ctx)
{
    Type *value_type  = type_check_expression(expr->data.assignment.value,  ctx);
    Type *target_type = type_check_expression(expr->data.assignment.target, ctx);

    require_assignable(value_type, target_type, expr, ctx);
    return target_type;
}

/* -----------------------------------------------------------------
 * Statement checkers
 * ----------------------------------------------------------------- */

bool
type_check_let_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.let_decl.name;
    ASTNode    *init = node->data.let_decl.initializer;
    ASTNode    *ann  = node->data.let_decl.type;

    /* Check for duplicate in current scope */
    if (scope_lookup_current(ctx->scope, name) != NULL) {
        semantic_error(ctx, node,
            "Redeclaration of '%s' in the same scope", name);
        return false;
    }

    /*
     * Detect ClaimSlot / ClaimSecureSlot calls so we can register
     * a SYMBOL_SLOT with proper metadata.
     */
    bool is_slot_decl = false;
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *callee_name =
            init->data.call.callee->data.identifier.name;
        BuiltinKind bk = builtin_resolve(callee_name);

        if (bk == BUILTIN_CLAIM_SLOT || bk == BUILTIN_CLAIM_SECURE_SLOT) {
            is_slot_decl = true;
            bool is_secure = (bk == BUILTIN_CLAIM_SECURE_SLOT);

            /* Resolve inner type from annotation if present.
             * If the annotation is already a Slot type (e.g. Slot<String>),
             * use it directly instead of double-wrapping. */
            Type *slot_type = NULL;
            if (ann != NULL) {
                Type *ann_type = resolve_type_node(ann, ctx);
                if (ann_type->kind == TYPE_KIND_SLOT) {
                    slot_type = ann_type;
                    is_secure = ann_type->data.slot.is_secure;
                } else {
                    slot_type = type_create_slot(ann_type, is_secure);
                }
            } else {
                slot_type = type_create_slot(TYPE_INT, is_secure);
            }
            Symbol *sym       = symbol_create_slot(name, slot_type,
                                                    is_secure, NULL,
                                                    node->line, node->column);
            scope_declare(ctx->scope, sym);
            scope_register_slot(ctx->scope, sym);
            return true;
        }
    }

    /* Normal variable declaration with type inference */
    Type *init_type = (init != NULL)
                      ? type_check_expression(init, ctx)
                      : TYPE_VOID;

    Type *decl_type;
    
    /* Type inference: if no annotation, infer from initializer */
    if (ann != NULL) {
        /* Explicit type annotation */
        decl_type = resolve_type_node(ann, ctx);
        if (init != NULL) {
            if (init->type == AST_CALL
                && init->data.call.callee->type == AST_IDENTIFIER
                && strcmp(init->data.call.callee->data.identifier.name,
                          "BoxArray") == 0) {
                init_type = decl_type;
            }
            require_assignable(init_type, decl_type, init, ctx);
        }
    } else if (init != NULL) {
        /* Infer type from initializer */
        decl_type = init_type;
        
        /* For generic types like Box<T>, Array<T>, Result<T,E>, 
           ensure the inferred type is concrete */
        if (init_type->kind == TYPE_KIND_GENERIC) {
            semantic_error(ctx, init, 
                "Cannot infer type: generic parameter '%s' is ambiguous. "
                "Please provide a type annotation.", init_type->name);
            decl_type = TYPE_UNKNOWN;
        }
    } else {
        /* No annotation and no initializer */
        semantic_error(ctx, node,
            "Cannot infer type: provide a type annotation or initializer");
        decl_type = TYPE_UNKNOWN;
    }

    Symbol *sym = symbol_create_variable(name, decl_type,
                                          node->line, node->column);
    if (!is_slot_decl)
        scope_declare(ctx->scope, sym);

    return !ctx->has_error;
}

bool
type_check_return_stmt(ASTNode *node, SemanticContext *ctx)
{
    Type *ret_type = TYPE_VOID;
    if (node->data.return_stmt.value != NULL)
        ret_type = type_check_expression(node->data.return_stmt.value, ctx);

    if (ctx->current_return != NULL)
        require_assignable(ret_type, ctx->current_return, node, ctx);

    return !ctx->has_error;
}

bool
type_check_if_stmt(ASTNode *node, SemanticContext *ctx)
{
    Type *cond = type_check_expression(node->data.if_stmt.condition, ctx);
    if (!type_equals(cond, TYPE_BOOL)) {
        semantic_error(ctx, node,
            "If condition must be Bool, got '%s'", cond->name);
    }

    scope_enter(&ctx->scope, SCOPE_BLOCK);
    type_check_block(node->data.if_stmt.then_branch, ctx);
    scope_exit(&ctx->scope);

    if (node->data.if_stmt.else_branch != NULL) {
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        type_check_statement(node->data.if_stmt.else_branch, ctx);
        scope_exit(&ctx->scope);
    }

    return !ctx->has_error;
}

bool
type_check_for_loop(ASTNode *node, SemanticContext *ctx)
{
    scope_enter(&ctx->scope, SCOPE_BLOCK);

    /* Register loop variable as Int */
    Symbol *loop_var = symbol_create_variable(
        node->data.for_loop.variable, TYPE_INT,
        node->line, node->column);
    scope_declare(ctx->scope, loop_var);

    if (node->data.for_loop.range_start != NULL) {
        Type *t = type_check_expression(node->data.for_loop.range_start, ctx);
        require_assignable(t, TYPE_INT, node->data.for_loop.range_start, ctx);
    }
    if (node->data.for_loop.range_end != NULL) {
        Type *t = type_check_expression(node->data.for_loop.range_end, ctx);
        require_assignable(t, TYPE_INT, node->data.for_loop.range_end, ctx);
    }

    type_check_block(node->data.for_loop.body, ctx);
    scope_exit(&ctx->scope);
    return !ctx->has_error;
}

bool
type_check_while_loop(ASTNode *node, SemanticContext *ctx)
{
    Type *cond = type_check_expression(node->data.while_loop.condition, ctx);
    if (!type_equals(cond, TYPE_BOOL)) {
        semantic_error(ctx, node,
            "While condition must be Bool, got '%s'", cond->name);
    }

    scope_enter(&ctx->scope, SCOPE_BLOCK);
    type_check_block(node->data.while_loop.body, ctx);
    scope_exit(&ctx->scope);

    return !ctx->has_error;
}

bool
type_check_match_stmt(ASTNode *node, SemanticContext *ctx)
{
    Type *subj_type = type_check_expression(node->data.match_stmt.subject, ctx);

    for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
        ASTNode *mc = node->data.match_stmt.cases[i];

        scope_enter(&ctx->scope, SCOPE_BLOCK);

        /* Check pattern type compatibility */
        if (mc->data.match_case.pattern != NULL) {
            Type *pat_type = type_check_expression(mc->data.match_case.pattern, ctx);
            if (!type_is_assignable(pat_type, subj_type) &&
                !type_is_assignable(subj_type, pat_type)) {
                semantic_error(ctx, mc->data.match_case.pattern,
                    "Case pattern type '%s' incompatible with match subject '%s'",
                    pat_type->name, subj_type->name);
            }
        }

        /* Guard must be Bool */
        if (mc->data.match_case.guard != NULL) {
            Type *guard_type = type_check_expression(mc->data.match_case.guard, ctx);
            if (!type_equals(guard_type, TYPE_BOOL)) {
                semantic_error(ctx, mc->data.match_case.guard,
                    "Case guard must be Bool, got '%s'", guard_type->name);
            }
        }

        type_check_block(mc->data.match_case.body, ctx);
        scope_exit(&ctx->scope);
    }

    /* Check default body */
    if (node->data.match_stmt.default_body != NULL) {
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        type_check_block(node->data.match_stmt.default_body, ctx);
        scope_exit(&ctx->scope);
    }

    return !ctx->has_error;
}

bool
type_check_with_stmt(ASTNode *node, SemanticContext *ctx)
{
    scope_enter(&ctx->scope, SCOPE_WITH);

    ASTNode *slot_type_node = node->data.with_stmt.slot_type;
    const char *alias       = node->data.with_stmt.alias;
    bool is_secure          = node->data.with_stmt.is_secure;

    Type *inner = resolve_type_node(slot_type_node, ctx);
    Type *slot_type = type_create_slot(inner, is_secure);

    Symbol *sym = symbol_create_slot(alias, slot_type, is_secure, NULL,
                                      node->line, node->column);
    scope_declare(ctx->scope, sym);
    scope_register_slot(ctx->scope, sym);

    type_check_block(node->data.with_stmt.body, ctx);

    /* Auto-release all slots registered in this with-scope */
    scope_auto_release_slots(ctx->scope);
    scope_exit(&ctx->scope);
    return !ctx->has_error;
}

bool
type_check_parallel_block(ASTNode *node, SemanticContext *ctx)
{
    bool prev_parallel = ctx->in_parallel;
    ctx->in_parallel   = true;

    for (size_t i = 0; i < node->data.parallel.task_count; i++) {
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        type_check_statement(node->data.parallel.tasks[i], ctx);
        scope_exit(&ctx->scope);
    }

    ctx->in_parallel = prev_parallel;
    return !ctx->has_error;
}

bool
type_check_ability_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.ability_decl.name;

    /* Register ability as a symbol so roles can reference it */
    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_ABILITY;
    sym->type = TYPE_VOID; /* Abilities don't have a concrete type */
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL) {
        semantic_error(ctx, node, "Redeclaration of ability '%s'", name);
        symbol_destroy(sym);
        return false;
    }
    scope_declare(ctx->scope, sym);

    /* Check require fields have valid types */
    for (size_t i = 0; i < node->data.ability_decl.require_count; i++) {
        ASTNode *req = node->data.ability_decl.require_fields[i];
        resolve_type_node(req->data.require_field.type, ctx);
    }

    /* Check method signatures */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.ability_decl.method_count; i++) {
        ASTNode *method = node->data.ability_decl.methods[i];
        /* Only type-check methods that have a body */
        if (method->data.func_decl.body != NULL) {
            type_check_func_decl(method, ctx);
        } else {
            /* Abstract method — just validate the signature types */
            if (method->data.func_decl.return_type != NULL)
                resolve_type_node(method->data.func_decl.return_type, ctx);
            for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
                if (method->data.func_decl.params[j]->type != NULL)
                    resolve_type_node(method->data.func_decl.params[j]->type, ctx);
            }
        }
    }
    scope_exit(&ctx->scope);

    return !ctx->has_error;
}

bool
type_check_role_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.role_decl.name;

    /* Register role as a symbol */
    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_ROLE;
    sym->type = TYPE_VOID;
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL) {
        semantic_error(ctx, node, "Redeclaration of role '%s'", name);
        symbol_destroy(sym);
        return false;
    }
    scope_declare(ctx->scope, sym);

    /* Check for_type exists */
    if (node->data.role_decl.for_type != NULL) {
        resolve_type_node(node->data.role_decl.for_type, ctx);
    }

    /* Check includes reference existing roles */
    for (size_t i = 0; i < node->data.role_decl.include_count; i++) {
        ASTNode *inc = node->data.role_decl.includes[i];
        const char *role_name = inc->data.include_stmt.role_name;
        Symbol *role_sym = scope_lookup(ctx->scope, role_name);
        if (role_sym == NULL) {
            semantic_warning(ctx, inc,
                "Included role '%s' not found in current scope", role_name);
        }
    }

    /* Check impl ability blocks */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.role_decl.impl_count; i++) {
        ASTNode *impl = node->data.role_decl.impl_abilities[i];

        if (impl->type == AST_IMPL_ABILITY) {
            /* Check that the ability exists */
            if (impl->data.impl_ability.ability_name != NULL) {
                Symbol *ab = scope_lookup(ctx->scope,
                    impl->data.impl_ability.ability_name);
                if (ab == NULL || ab->kind != SYMBOL_ABILITY) {
                    semantic_warning(ctx, impl,
                        "Ability '%s' not found in current scope",
                        impl->data.impl_ability.ability_name);
                }
            }

            /* Type-check each method implementation */
            for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
                type_check_func_decl(impl->data.impl_ability.methods[j], ctx);
            }
        } else if (impl->type == AST_OVERRIDE_FUNC) {
            /* Type-check the overridden function */
            if (impl->data.override_func.func_decl != NULL)
                type_check_func_decl(impl->data.override_func.func_decl, ctx);
        }
    }
    scope_exit(&ctx->scope);

    return !ctx->has_error;
}

bool
type_check_party_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.party_decl.name;

    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_PARTY;
    sym->type = TYPE_VOID;
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL) {
        semantic_error(ctx, node, "Redeclaration of party '%s'", name);
        symbol_destroy(sym);
        return false;
    }
    scope_declare(ctx->scope, sym);

    /* Check role slot ability references */
    for (size_t i = 0; i < node->data.party_decl.role_count; i++) {
        ASTNode *rs = node->data.party_decl.role_slots[i];

        /* dyn slots require at least one ability for vtable dispatch */
        if (rs->data.role_slot.is_dynamic &&
            rs->data.role_slot.ability_count == 0) {
            semantic_error(ctx, rs,
                "Dynamic role slot '%s' requires at least one ability type",
                rs->data.role_slot.slot_name);
        }

        for (size_t j = 0; j < rs->data.role_slot.ability_count; j++) {
            ASTNode *ab_type = rs->data.role_slot.required_abilities[j];
            if (ab_type != NULL && ab_type->data.type.name != NULL) {
                Symbol *ab = scope_lookup(ctx->scope, ab_type->data.type.name);
                if (ab == NULL || ab->kind != SYMBOL_ABILITY) {
                    semantic_warning(ctx, rs,
                        "Ability '%s' not found for role slot '%s'",
                        ab_type->data.type.name,
                        rs->data.role_slot.slot_name);
                }
            }
        }
    }

    /* Check shared fields */
    for (size_t i = 0; i < node->data.party_decl.shared_count; i++) {
        ASTNode *shared = node->data.party_decl.shared_fields[i];
        if (shared->data.party_shared.type != NULL)
            resolve_type_node(shared->data.party_shared.type, ctx);
        if (shared->data.party_shared.initializer != NULL)
            type_check_expression(shared->data.party_shared.initializer, ctx);
    }

    /* Check methods */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.party_decl.method_count; i++) {
        type_check_func_decl(node->data.party_decl.methods[i], ctx);
    }
    scope_exit(&ctx->scope);

    return !ctx->has_error;
}

bool
type_check_systemic_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.systemic_decl.name;

    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_SYSTEMIC;
    sym->type = TYPE_VOID;
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL) {
        semantic_error(ctx, node, "Redeclaration of systemic '%s'", name);
        symbol_destroy(sym);
        return false;
    }
    scope_declare(ctx->scope, sym);

    /* Check party slot references */
    for (size_t i = 0; i < node->data.systemic_decl.party_count; i++) {
        ASTNode *ps = node->data.systemic_decl.party_slots[i];
        if (ps->data.systemic_slot.party_type != NULL) {
            Symbol *party = scope_lookup(ctx->scope,
                ps->data.systemic_slot.party_type);
            if (party == NULL || party->kind != SYMBOL_PARTY) {
                semantic_warning(ctx, ps,
                    "Party type '%s' not found for slot '%s'",
                    ps->data.systemic_slot.party_type,
                    ps->data.systemic_slot.slot_name);
            }
        }
    }

    /* Check shared fields */
    for (size_t i = 0; i < node->data.systemic_decl.shared_count; i++) {
        ASTNode *shared = node->data.systemic_decl.shared_fields[i];
        if (shared->data.party_shared.type != NULL)
            resolve_type_node(shared->data.party_shared.type, ctx);
        if (shared->data.party_shared.initializer != NULL)
            type_check_expression(shared->data.party_shared.initializer, ctx);
    }

    /* Check methods */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.systemic_decl.method_count; i++) {
        type_check_func_decl(node->data.systemic_decl.methods[i], ctx);
    }
    scope_exit(&ctx->scope);

    return !ctx->has_error;
}

bool
type_check_world_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.world_decl.name;

    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_WORLD;
    sym->type = TYPE_VOID;
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL) {
        semantic_error(ctx, node, "Redeclaration of world '%s'", name);
        symbol_destroy(sym);
        return false;
    }
    scope_declare(ctx->scope, sym);

    /* Check systemic references */
    for (size_t i = 0; i < node->data.world_decl.systemic_count; i++) {
        ASTNode *ws = node->data.world_decl.systemics[i];
        if (ws->data.world_systemic.systemic_type != NULL) {
            Symbol *sys = scope_lookup(ctx->scope,
                ws->data.world_systemic.systemic_type);
            if (sys == NULL || sys->kind != SYMBOL_SYSTEMIC) {
                semantic_warning(ctx, ws,
                    "Systemic type '%s' not found for slot '%s'",
                    ws->data.world_systemic.systemic_type,
                    ws->data.world_systemic.slot_name);
            }
        }
    }

    /* Check shared fields */
    for (size_t i = 0; i < node->data.world_decl.shared_count; i++) {
        ASTNode *shared = node->data.world_decl.shared_fields[i];
        if (shared->data.party_shared.type != NULL)
            resolve_type_node(shared->data.party_shared.type, ctx);
        if (shared->data.party_shared.initializer != NULL)
            type_check_expression(shared->data.party_shared.initializer, ctx);
    }

    /* Check methods */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.world_decl.method_count; i++) {
        type_check_func_decl(node->data.world_decl.methods[i], ctx);
    }
    scope_exit(&ctx->scope);

    return !ctx->has_error;
}

/* -----------------------------------------------------------------
 * Async system type checkers
 * ----------------------------------------------------------------- */

bool
type_check_actor_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.actor_decl.name;

    /* Register actor as a symbol */
    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_ACTOR;
    sym->type = TYPE_VOID;
    sym->decl_line = node->line;
    sym->decl_col  = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL) {
        semantic_error(ctx, node,
            "Duplicate actor declaration '%s'", name);
        symbol_destroy(sym);
        return false;
    }
    scope_declare(ctx->scope, sym);

    /* Check fields */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.actor_decl.field_count; i++) {
        ClassField *f = node->data.actor_decl.fields[i];
        if (f && f->type) {
            Type *ft = resolve_type_node(f->type, ctx);
            if (ft && f->name) {
                Symbol *fsym = symbol_create_variable(f->name, ft,
                                                       node->line, node->column);
                scope_declare(ctx->scope, fsym);
            }
        }
    }

    /* Check methods (implicitly async) */
    bool saved_async = ctx->in_async_func;
    ctx->in_async_func = true;
    for (size_t i = 0; i < node->data.actor_decl.method_count; i++) {
        ASTNode *method = node->data.actor_decl.methods[i];
        if (method) {
            scope_enter(&ctx->scope, SCOPE_FUNCTION);
            /* Register self + params */
            Symbol *self_sym = symbol_create_variable("self", TYPE_UNKNOWN,
                                                        node->line, node->column);
            scope_declare(ctx->scope, self_sym);
            for (size_t k = 0; k < method->data.async_func_decl.param_count; k++) {
                FuncParam *p = method->data.async_func_decl.params[k];
                if (p == NULL || (p->type == NULL && strcmp(p->name, "self") == 0))
                    continue;
                Type *pt = (p->type != NULL) ? resolve_type_node(p->type, ctx) : TYPE_UNKNOWN;
                Symbol *ps = symbol_create_variable(p->name, pt,
                                                      node->line, node->column);
                scope_declare(ctx->scope, ps);
            }
            if (method->data.async_func_decl.body)
                type_check_block(method->data.async_func_decl.body, ctx);
            scope_exit(&ctx->scope);
        }
    }
    ctx->in_async_func = saved_async;
    scope_exit(&ctx->scope);

    return !ctx->has_error;
}

bool
type_check_async_block(ASTNode *node, SemanticContext *ctx)
{
    bool saved_async = ctx->in_async_func;
    ctx->in_async_func = true;

    for (size_t i = 0; i < node->data.async_block.statement_count; i++) {
        type_check_statement(node->data.async_block.statements[i], ctx);
    }

    ctx->in_async_func = saved_async;
    return !ctx->has_error;
}

bool
type_check_select_stmt(ASTNode *node, SemanticContext *ctx)
{
    /* Each case should involve channel operations */
    for (size_t i = 0; i < node->data.select_stmt.case_count; i++) {
        ASTNode *c = node->data.select_stmt.cases[i];
        if (c) type_check_statement(c, ctx);
    }

    if (node->data.select_stmt.default_case)
        type_check_statement(node->data.select_stmt.default_case, ctx);

    return !ctx->has_error;
}

Type *
type_check_spawn_expr(ASTNode *expr, SemanticContext *ctx)
{
    /* Type-check the spawned function/expression */
    Type *inner = type_check_expression(expr->data.spawn_expr.function, ctx);
    Type *args[1] = { inner != NULL ? inner : TYPE_UNKNOWN };
    return type_create_constructed(TYPE_FUTURE, args, 1);
}

Type *
type_check_channel_send(ASTNode *expr, SemanticContext *ctx)
{
    /* Check channel and value types */
    Type *channel_type = type_check_expression(expr->data.channel_send.channel, ctx);
    Type *value_type = type_check_expression(expr->data.channel_send.value, ctx);
    if (channel_type == NULL
        || channel_type->kind != TYPE_KIND_CONSTRUCTED
        || !type_equals(channel_type->data.constructed.constructor, TYPE_CHANNEL)
        || channel_type->data.constructed.arg_count != 1) {
        semantic_error(ctx, expr->data.channel_send.channel,
            "Channel send requires Channel<T>, got '%s'",
            channel_type != NULL ? channel_type->name : "<null>");
        return TYPE_VOID;
    }
    require_assignable(value_type, channel_type->data.constructed.args[0],
        expr->data.channel_send.value, ctx);
    return TYPE_VOID;
}

Type *
type_check_channel_recv(ASTNode *expr, SemanticContext *ctx)
{
    Type *channel_type = type_check_expression(expr->data.channel_recv.channel, ctx);
    if (channel_type == NULL
        || channel_type->kind != TYPE_KIND_CONSTRUCTED
        || !type_equals(channel_type->data.constructed.constructor, TYPE_CHANNEL)
        || channel_type->data.constructed.arg_count != 1) {
        semantic_error(ctx, expr->data.channel_recv.channel,
            "Channel recv requires Channel<T>, got '%s'",
            channel_type != NULL ? channel_type->name : "<null>");
        return TYPE_UNKNOWN;
    }
    return channel_type->data.constructed.args[0];
}

bool
type_check_statement(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL)
        return true;

    switch (node->type) {
    case AST_LET_DECL:
        return type_check_let_decl(node, ctx);
    case AST_FUNC_DECL:
        return type_check_func_decl(node, ctx);
    case AST_CLASS_DECL:
        return type_check_class_decl(node, ctx);
    case AST_EXTERN_BLOCK:
        return type_check_extern_block(node, ctx);
    case AST_IF_STMT:
        return type_check_if_stmt(node, ctx);
    case AST_FOR_LOOP:
        return type_check_for_loop(node, ctx);
    case AST_WHILE_LOOP:
        return type_check_while_loop(node, ctx);
    case AST_MATCH_STMT:
        return type_check_match_stmt(node, ctx);
    case AST_RETURN:
        return type_check_return_stmt(node, ctx);
    case AST_WITH_STMT:
        return type_check_with_stmt(node, ctx);
    case AST_PARALLEL_BLOCK:
        return type_check_parallel_block(node, ctx);
    case AST_ABILITY_DECL:
        return type_check_ability_decl(node, ctx);
    case AST_ROLE_DECL:
        return type_check_role_decl(node, ctx);
    case AST_PARTY_DECL:
        return type_check_party_decl(node, ctx);
    case AST_SYSTEMIC_DECL:
        return type_check_systemic_decl(node, ctx);
    case AST_WORLD_DECL:
        return type_check_world_decl(node, ctx);
    case AST_ACTOR_DECL:
        return type_check_actor_decl(node, ctx);
    case AST_ASYNC_BLOCK:
        return type_check_async_block(node, ctx);
    case AST_SELECT_STMT:
        return type_check_select_stmt(node, ctx);
    case AST_BLOCK:
        return type_check_block(node, ctx);
    case AST_IMPORT_DECL:
        /* Already resolved by driver — skip */
        return true;
    case AST_UNSAFE_BLOCK:
        /* Type-check body normally; safety constraints relaxed at codegen */
        if (node->data.unsafe_block.body != NULL)
            type_check_block(node->data.unsafe_block.body, ctx);
        return !ctx->has_error;
    case AST_DEFER_STMT:
        /* Type-check deferred body */
        if (node->data.defer_stmt.body != NULL)
            type_check_block(node->data.defer_stmt.body, ctx);
        return !ctx->has_error;
    default:
        /* Expression statement */
        type_check_expression(node, ctx);
        return !ctx->has_error;
    }
}

bool
type_check_block(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL)
        return true;

    if (node->type == AST_BLOCK) {
        for (size_t i = 0; i < node->data.block.count; i++)
            type_check_statement(node->data.block.statements[i], ctx);
    } else {
        type_check_statement(node, ctx);
    }

    return !ctx->has_error;
}

bool
type_check_func_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.func_decl.name;

    /* If the function has generic parameters (<T, U, ...>),
     * register them as opaque types in a temporary scope so that
     * resolve_type_node("T") succeeds for params and return type. */
    bool has_generics = (node->data.func_decl.generic_params != NULL
                         && node->data.func_decl.generic_params->count > 0);
    if (has_generics) {
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        GenericParams *gp = node->data.func_decl.generic_params;
        for (size_t gi = 0; gi < gp->count; gi++) {
            if (gp->params[gi] == NULL || gp->params[gi]->name == NULL)
                continue;
            Type *tp = calloc(1, sizeof(Type));
            if (tp != NULL) {
                tp->kind = TYPE_KIND_CLASS;
                tp->name = pergyra_strdup(gp->params[gi]->name);
            }
            Symbol *s = symbol_create_variable(
                gp->params[gi]->name,
                tp != NULL ? tp : TYPE_UNKNOWN,
                node->line, node->column);
            s->kind = SYMBOL_CLASS;
            scope_declare(ctx->scope, s);
        }
    }

    /* Build parameter types for the function type */
    size_t   param_count = node->data.func_decl.param_count;
    Type   **param_types = NULL;

    if (param_count > 0) {
        param_types = calloc(param_count, sizeof(Type *));
        if (param_types == NULL) {
            if (has_generics) scope_exit(&ctx->scope);
            return false;
        }
    }

    Type *return_type = TYPE_VOID;
    if (node->data.func_decl.return_type != NULL)
        return_type = resolve_type_node(node->data.func_decl.return_type, ctx);

    for (size_t i = 0; i < param_count; i++) {
        param_types[i] = resolve_type_node(
            node->data.func_decl.params[i]->type, ctx);
    }

    Type *func_type = type_create_function(param_types, param_count,
                                            return_type);
    free(param_types);

    Symbol *func_sym = symbol_create_function(name, func_type,
                                               node->line, node->column);
    /* If a forward-declaration placeholder exists from Pass 1,
       update its type instead of re-declaring. */
    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL && existing->kind == SYMBOL_FUNCTION) {
        existing->type = func_type;
        symbol_destroy(func_sym);
    } else if (!scope_declare(ctx->scope, func_sym)) {
        semantic_error(ctx, node, "Redeclaration of function '%s'", name);
        symbol_destroy(func_sym);
        return false;
    }

    /* Close the temporary generic-params scope (if opened) before
     * entering the real function scope — the function scope will
     * re-register the generic params so they're visible in the body. */
    if (has_generics)
        scope_exit(&ctx->scope);

    /* Check body in new function scope */
    scope_enter(&ctx->scope, SCOPE_FUNCTION);

    /* Re-register generic type params inside the function scope */
    if (has_generics) {
        GenericParams *gp = node->data.func_decl.generic_params;
        for (size_t gi = 0; gi < gp->count; gi++) {
            if (gp->params[gi] == NULL || gp->params[gi]->name == NULL)
                continue;
            Type *tp = calloc(1, sizeof(Type));
            if (tp != NULL) {
                tp->kind = TYPE_KIND_CLASS;
                tp->name = pergyra_strdup(gp->params[gi]->name);
            }
            Symbol *s = symbol_create_variable(
                gp->params[gi]->name,
                tp != NULL ? tp : TYPE_UNKNOWN,
                node->line, node->column);
            s->kind = SYMBOL_CLASS;
            scope_declare(ctx->scope, s);
        }
    }

    Type *prev_return  = ctx->current_return;
    ctx->current_return = return_type;

    /* Register parameters */
    for (size_t i = 0; i < param_count; i++) {
        Type *pt = func_type->data.function.param_types[i];
        Symbol *p = symbol_create_variable(
            node->data.func_decl.params[i]->name, pt,
            node->line, node->column);
        scope_declare(ctx->scope, p);
    }

    if (node->data.func_decl.body != NULL)
        type_check_block(node->data.func_decl.body, ctx);

    ctx->current_return = prev_return;
    scope_exit(&ctx->scope);
    return !ctx->has_error;
}

bool
type_check_class_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.class_decl.name;

    Type *class_type = calloc(1, sizeof(Type));
    if (class_type == NULL)
        return false;
    class_type->kind = TYPE_KIND_CLASS;
    class_type->name = pergyra_strdup(name);

    Symbol *class_sym = symbol_create_function(name, class_type,
                                                node->line, node->column);
    class_sym->kind = SYMBOL_CLASS;

    if (!scope_declare(ctx->scope, class_sym)) {
        semantic_error(ctx, node, "Redeclaration of class '%s'", name);
        symbol_destroy(class_sym);
        return false;
    }

    /* Check methods */
    scope_enter(&ctx->scope, SCOPE_CLASS);
    for (size_t i = 0; i < node->data.class_decl.method_count; i++)
        type_check_func_decl(node->data.class_decl.methods[i], ctx);
    scope_exit(&ctx->scope);

    return !ctx->has_error;
}

bool
type_check_extern_block(ASTNode *node, SemanticContext *ctx)
{
    for (size_t i = 0; i < node->data.extern_block.count; i++) {
        ASTNode *decl = node->data.extern_block.declarations[i];
        if (decl != NULL && decl->type == AST_FUNC_DECL)
            type_check_func_decl(decl, ctx);
    }
    return !ctx->has_error;
}

/* -----------------------------------------------------------------
 * Program entry point
 * ----------------------------------------------------------------- */

bool
type_check_program(ASTNode *program, SemanticContext *ctx)
{
    if (program == NULL || program->type != AST_PROGRAM)
        return false;

    /*
     * Pass 1: collect all top-level function and class names
     * so that forward references within the same file work.
     */
    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt->type == AST_FUNC_DECL) {
            const char *fname = stmt->data.func_decl.name;
            if (scope_lookup_current(ctx->scope, fname) == NULL) {
                /* Forward-declare with correct param count so that
                 * call-site arity checks pass before Pass 2. */
                size_t fpc = stmt->data.func_decl.param_count;
                /* Exclude implicit 'self' param from count */
                size_t real_pc = 0;
                for (size_t j = 0; j < fpc; j++) {
                    FuncParam *p = stmt->data.func_decl.params[j];
                    if (p->type == NULL && strcmp(p->name, "self") == 0)
                        continue;
                    real_pc++;
                }
                Type **ptypes = calloc(real_pc > 0 ? real_pc : 1,
                                         sizeof(Type *));
                for (size_t j = 0; j < real_pc; j++)
                    ptypes[j] = TYPE_UNKNOWN;
                Type *ret = TYPE_VOID;
                if (stmt->data.func_decl.return_type != NULL) {
                    /* Try to resolve return type; use TYPE_UNKNOWN on failure
                     * (e.g., generic return type 'T' not in scope yet). */
                    size_t saved_diag = ctx->diagnostic_count;
                    bool saved_err = ctx->has_error;
                    ret = resolve_type_node(stmt->data.func_decl.return_type, ctx);
                    if (ctx->diagnostic_count > saved_diag) {
                        /* Roll back the diagnostic — Pass 2 will re-check */
                        ctx->diagnostic_count = saved_diag;
                        ctx->has_error = saved_err;
                        ret = TYPE_UNKNOWN;
                    }
                }
                Type *placeholder = type_create_function(ptypes, real_pc, ret);
                free(ptypes);
                Symbol *s = symbol_create_function(fname, placeholder,
                                                    stmt->line, stmt->column);
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_EVENT_DECL) {
            const char *ename = stmt->data.event_decl.name;
            if (scope_lookup_current(ctx->scope, ename) == NULL) {
                size_t epc = stmt->data.event_decl.param_count;
                Type **eptypes = calloc(epc > 0 ? epc : 1, sizeof(Type *));
                for (size_t j = 0; j < epc; j++) {
                    ASTNode *p = stmt->data.event_decl.params[j];
                    if (p->data.let_decl.type != NULL)
                        eptypes[j] = resolve_type_node(p->data.let_decl.type, ctx);
                    else
                        eptypes[j] = TYPE_INT;
                }
                Type *evt_ft = type_create_function(eptypes, epc, TYPE_VOID);
                free(eptypes);
                Symbol *s = symbol_create_function(ename, evt_ft,
                                                    stmt->line, stmt->column);
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_EXTERN_BLOCK) {
            for (size_t j = 0; j < stmt->data.extern_block.count; j++) {
                ASTNode *decl = stmt->data.extern_block.declarations[j];
                if (decl == NULL || decl->type != AST_FUNC_DECL)
                    continue;
                const char *fname = decl->data.func_decl.name;
                if (scope_lookup_current(ctx->scope, fname) == NULL) {
                    Type *placeholder = type_create_function(NULL, 0, TYPE_VOID);
                    Symbol *s = symbol_create_function(fname, placeholder,
                                                        decl->line, decl->column);
                    scope_declare(ctx->scope, s);
                }
            }
        }
    }

    /*
     * Pass 2: full type-check
     */
    for (size_t i = 0; i < program->data.program.count; i++)
        type_check_statement(program->data.program.statements[i], ctx);

    return !ctx->has_error;
}
