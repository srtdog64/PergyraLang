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
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, ap);

    for (size_t i = 0; i < ctx->diagnostic_count; i++) {
        Diagnostic *existing = ctx->diagnostics[i];
        if (existing == NULL)
            continue;
        if (existing->level != level)
            continue;
        if (existing->line != (node ? node->line : 0)
            || existing->col != (node ? node->column : 0)) {
            continue;
        }
        if (existing->message != NULL && strcmp(existing->message, buf) == 0)
            return;
    }

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
    if (strcmp(name, "QubitSlot") == 0) return TYPE_QUBIT;
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

static bool
type_is_qubit(const Type *type)
{
    if (type == NULL)
        return false;
    if (TYPE_QUBIT != NULL && type_equals(type, TYPE_QUBIT))
        return true;
    return type->name != NULL && strcmp(type->name, "QubitSlot") == 0;
}

static bool
expr_is_qubit_claim(const ASTNode *expr)
{
    return expr != NULL
        && expr->type == AST_CALL
        && expr->data.call.callee != NULL
        && expr->data.call.callee->type == AST_IDENTIFIER
        && expr->data.call.callee->data.identifier.name != NULL
        && strcmp(expr->data.call.callee->data.identifier.name, "ClaimQubit") == 0;
}

typedef struct
{
    Symbol **symbols;
    bool    *states;
    size_t   count;
} QubitConsumeSnapshot;

typedef enum
{
    FLOW_NONE        = 0,
    FLOW_FALLTHROUGH = 1 << 0,
    FLOW_BREAK       = 1 << 1,
    FLOW_CONTINUE    = 1 << 2,
    FLOW_RETURN      = 1 << 3
} FlowFlags;

typedef struct
{
    QubitConsumeSnapshot break_states;
    QubitConsumeSnapshot continue_states;
    bool                 has_break_states;
    bool                 has_continue_states;
} LoopFlowState;

static FlowFlags type_check_statement_flow(ASTNode *node,
                                           SemanticContext *ctx,
                                           LoopFlowState *loop_flow);
static FlowFlags type_check_block_flow(ASTNode *node,
                                       SemanticContext *ctx,
                                       LoopFlowState *loop_flow);
static FlowFlags type_check_if_stmt_flow(ASTNode *node,
                                         SemanticContext *ctx,
                                         LoopFlowState *loop_flow);
static FlowFlags type_check_match_stmt_flow(ASTNode *node,
                                            SemanticContext *ctx,
                                            LoopFlowState *loop_flow);
static FlowFlags type_check_with_stmt_flow(ASTNode *node,
                                           SemanticContext *ctx,
                                           LoopFlowState *loop_flow);

static QubitConsumeSnapshot
snapshot_qubit_states(SemanticContext *ctx)
{
    QubitConsumeSnapshot snap = {0};
    Scope *scope = ctx != NULL ? ctx->scope : NULL;

    while (scope != NULL) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *sym = scope->symbols[i];
            if (sym == NULL || !type_is_qubit(sym->type))
                continue;

            Symbol **new_symbols = realloc(snap.symbols,
                (snap.count + 1) * sizeof(Symbol *));
            bool *new_states = realloc(snap.states,
                (snap.count + 1) * sizeof(bool));
            if (new_symbols == NULL || new_states == NULL) {
                free(new_symbols);
                free(new_states);
                free(snap.symbols);
                free(snap.states);
                snap.symbols = NULL;
                snap.states = NULL;
                snap.count = 0;
                return snap;
            }

            snap.symbols = new_symbols;
            snap.states = new_states;
            snap.symbols[snap.count] = sym;
            snap.states[snap.count] = sym->is_consumed;
            snap.count++;
        }
        scope = scope->parent;
    }

    return snap;
}

static void
restore_qubit_states(const QubitConsumeSnapshot *snap)
{
    if (snap == NULL)
        return;
    for (size_t i = 0; i < snap->count; i++) {
        if (snap->symbols[i] != NULL)
            snap->symbols[i]->is_consumed = snap->states[i];
    }
}

static void
merge_qubit_states_or(QubitConsumeSnapshot *dst,
                      const QubitConsumeSnapshot *src)
{
    if (dst == NULL || src == NULL)
        return;
    size_t count = dst->count < src->count ? dst->count : src->count;
    for (size_t i = 0; i < count; i++)
        dst->states[i] = dst->states[i] || src->states[i];
}

static void
destroy_qubit_snapshot(QubitConsumeSnapshot *snap)
{
    if (snap == NULL)
        return;
    free(snap->symbols);
    free(snap->states);
    snap->symbols = NULL;
    snap->states = NULL;
    snap->count = 0;
}

static bool
qubit_snapshots_equal(const QubitConsumeSnapshot *a,
                      const QubitConsumeSnapshot *b)
{
    if (a == NULL || b == NULL)
        return a == b;
    if (a->count != b->count)
        return false;
    for (size_t i = 0; i < a->count; i++) {
        if (a->symbols[i] != b->symbols[i])
            return false;
        if (a->states[i] != b->states[i])
            return false;
    }
    return true;
}

static size_t
for_loop_known_iteration_cap(const ASTNode *node, bool *known)
{
    if (known != NULL)
        *known = false;
    if (node == NULL
        || node->data.for_loop.range_start == NULL
        || node->data.for_loop.range_end == NULL) {
        return 0;
    }
    if (node->data.for_loop.range_start->type != AST_NUMBER
        || node->data.for_loop.range_end->type != AST_NUMBER) {
        return 0;
    }

    double start = node->data.for_loop.range_start->data.number.value;
    double end = node->data.for_loop.range_end->data.number.value;
    if (known != NULL)
        *known = true;
    if (end <= start)
        return 0;
    if ((end - start) <= 1.0)
        return 1;
    return 2;
}

static QubitConsumeSnapshot
copy_qubit_snapshot(const QubitConsumeSnapshot *src)
{
    QubitConsumeSnapshot dst = {0};
    if (src == NULL || src->count == 0)
        return dst;

    dst.symbols = calloc(src->count, sizeof(Symbol *));
    dst.states = calloc(src->count, sizeof(bool));
    if (dst.symbols == NULL || dst.states == NULL) {
        free(dst.symbols);
        free(dst.states);
        dst.symbols = NULL;
        dst.states = NULL;
        return dst;
    }

    memcpy(dst.symbols, src->symbols, src->count * sizeof(Symbol *));
    memcpy(dst.states, src->states, src->count * sizeof(bool));
    dst.count = src->count;
    return dst;
}

static void
merge_qubit_snapshots_or(QubitConsumeSnapshot *dst,
                         bool *dst_initialized,
                         const QubitConsumeSnapshot *src)
{
    if (dst == NULL || dst_initialized == NULL || src == NULL)
        return;

    if (!*dst_initialized) {
        *dst = copy_qubit_snapshot(src);
        *dst_initialized = true;
        return;
    }

    merge_qubit_states_or(dst, src);
}

static void
loop_flow_record(LoopFlowState *loop_flow,
                 bool is_break,
                 const QubitConsumeSnapshot *state)
{
    if (loop_flow == NULL || state == NULL)
        return;

    if (is_break) {
        merge_qubit_snapshots_or(&loop_flow->break_states,
                                 &loop_flow->has_break_states,
                                 state);
        return;
    }

    merge_qubit_snapshots_or(&loop_flow->continue_states,
                             &loop_flow->has_continue_states,
                             state);
}

static void
destroy_loop_flow_state(LoopFlowState *loop_flow)
{
    if (loop_flow == NULL)
        return;
    destroy_qubit_snapshot(&loop_flow->break_states);
    destroy_qubit_snapshot(&loop_flow->continue_states);
    loop_flow->has_break_states = false;
    loop_flow->has_continue_states = false;
}

static Symbol *
lookup_identifier_symbol(ASTNode *expr, SemanticContext *ctx)
{
    if (expr == NULL || expr->type != AST_IDENTIFIER
        || expr->data.identifier.name == NULL) {
        return NULL;
    }
    return scope_lookup(ctx->scope, expr->data.identifier.name);
}

static bool
consume_qubit_value(ASTNode *expr, SemanticContext *ctx, const char *action)
{
    Symbol *sym = lookup_identifier_symbol(expr, ctx);
    if (sym == NULL || !type_is_qubit(sym->type))
        return false;

    if (sym->is_consumed) {
        return false;
    }

    sym->is_consumed = true;
    sym->is_used = true;
    (void)action;
    return true;
}

static Type *
type_check_qubit_use(ASTNode *expr, SemanticContext *ctx)
{
    if (expr != NULL && expr->type == AST_IDENTIFIER) {
        Symbol *sym = lookup_identifier_symbol(expr, ctx);
        if (sym == NULL) {
            semantic_error(ctx, expr,
                "Undefined symbol '%s'",
                expr->data.identifier.name);
            return TYPE_UNKNOWN;
        }
        if (!type_is_qubit(sym->type)) {
            semantic_error(ctx, expr,
                "Expected QubitSlot, got '%s'", sym->type->name);
            return TYPE_UNKNOWN;
        }
        if (sym->is_consumed) {
            semantic_error(ctx, expr,
                "QubitSlot '%s' was moved or released and cannot be used again",
                expr->data.identifier.name);
            return TYPE_UNKNOWN;
        }
        sym->is_used = true;
        return sym->type;
    }

    return type_check_expression(expr, ctx);
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

    /* Slot sugar: allow assigning T to Slot<T> (auto Claim+Write) */
    if (to->kind == TYPE_KIND_SLOT && to->data.slot.inner_type != NULL
        && type_is_assignable(from, to->data.slot.inner_type))
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
    /* I/O built-ins */
    /* String builtins — bypass as user functions (resolved in transpiler) */
    if (strcmp(name, "StringSplit")     == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "StringJoin")      == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "StringContains")  == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "StringReplace")   == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "Substring")       == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "StringTrim")      == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "ToUpper")         == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "ToLower")         == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "StringConcat")    == 0) return BUILTIN_NOT_BUILTIN;
    /* Quantum simulation builtins */
    if (strcmp(name, "ClaimQubit")      == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "Measure")         == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "Entangle")        == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "QubitState")      == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "IsCollapsed")     == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "ReleaseQubit")    == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "FileOpen")        == 0) return BUILTIN_FILE_OPEN;
    if (strcmp(name, "FileRead")        == 0) return BUILTIN_FILE_READ;
    if (strcmp(name, "FileWrite")       == 0) return BUILTIN_FILE_WRITE;
    if (strcmp(name, "FileClose")       == 0) return BUILTIN_FILE_CLOSE;
    if (strcmp(name, "ReadFile")        == 0) return BUILTIN_READ_FILE;
    if (strcmp(name, "WriteFile")       == 0) return BUILTIN_WRITE_FILE;
    if (strcmp(name, "Input")           == 0) return BUILTIN_INPUT;
    if (strcmp(name, "Print")           == 0) return BUILTIN_PRINT;
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

static const char *
operator_overload_suffix(TokenType op)
{
    switch (op) {
    case TOKEN_PLUS:          return "add";
    case TOKEN_MINUS:         return "sub";
    case TOKEN_STAR:          return "mul";
    case TOKEN_SLASH:         return "div";
    case TOKEN_PERCENT:       return "mod";
    case TOKEN_EQUAL:         return "eq";
    case TOKEN_NOT_EQUAL:     return "ne";
    case TOKEN_LESS:          return "lt";
    case TOKEN_LESS_EQUAL:    return "le";
    case TOKEN_GREATER:       return "gt";
    case TOKEN_GREATER_EQUAL: return "ge";
    default:                  return NULL;
    }
}

static bool
operator_method_name_matches(TokenType op, const char *name)
{
    static const struct {
        TokenType op;
        const char *names[10];
    } aliases[] = {
        { TOKEN_PLUS, { "Add", "add", "OperatorAdd", "operator_add", NULL } },
        { TOKEN_MINUS, { "Sub", "sub", "Subtract", "subtract",
                         "OperatorSub", "operator_sub", NULL } },
        { TOKEN_STAR, { "Mul", "mul", "Multiply", "multiply",
                        "OperatorMul", "operator_mul", NULL } },
        { TOKEN_SLASH, { "Div", "div", "Divide", "divide",
                         "OperatorDiv", "operator_div", NULL } },
        { TOKEN_PERCENT, { "Mod", "mod", "Modulo", "modulo",
                           "OperatorMod", "operator_mod", NULL } },
        { TOKEN_EQUAL, { "Eq", "eq", "Equal", "equal", "Equals", "equals",
                         "OperatorEq", "operator_eq", NULL } },
        { TOKEN_NOT_EQUAL, { "Ne", "ne", "NotEqual", "notEqual",
                             "NotEquals", "notEquals",
                             "OperatorNe", "operator_ne", NULL } },
        { TOKEN_LESS, { "Lt", "lt", "LessThan", "lessThan",
                        "OperatorLt", "operator_lt", NULL } },
        { TOKEN_LESS_EQUAL, { "Le", "le", "LessEqual", "lessEqual",
                              "LessThanOrEqual", "lessThanOrEqual",
                              "OperatorLe", "operator_le", NULL } },
        { TOKEN_GREATER, { "Gt", "gt", "GreaterThan", "greaterThan",
                           "OperatorGt", "operator_gt", NULL } },
        { TOKEN_GREATER_EQUAL, { "Ge", "ge", "GreaterEqual", "greaterEqual",
                                 "GreaterThanOrEqual", "greaterThanOrEqual",
                                 "OperatorGe", "operator_ge", NULL } },
    };

    if (name == NULL)
        return false;

    for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++) {
        if (aliases[i].op != op)
            continue;
        for (size_t j = 0; aliases[i].names[j] != NULL; j++) {
            if (strcmp(aliases[i].names[j], name) == 0)
                return true;
        }
        break;
    }

    return false;
}

static ASTNode *
find_role_decl_in_program(ASTNode *program, const char *role_name)
{
    if (program == NULL || program->type != AST_PROGRAM || role_name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt != NULL && stmt->type == AST_ROLE_DECL
            && stmt->data.role_decl.name != NULL
            && strcmp(stmt->data.role_decl.name, role_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static ASTNode *
find_role_operator_method(ASTNode *role, ASTNode *program, TokenType op,
                          int depth)
{
    if (role == NULL || role->type != AST_ROLE_DECL || depth > 16)
        return NULL;

    for (size_t i = 0; i < role->data.role_decl.impl_count; i++) {
        ASTNode *impl = role->data.role_decl.impl_abilities[i];
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;

        for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
            ASTNode *method = impl->data.impl_ability.methods[j];
            if (method != NULL && method->type == AST_FUNC_DECL
                && operator_method_name_matches(op, method->data.func_decl.name)) {
                return method;
            }
        }
    }

    for (size_t i = 0; i < role->data.role_decl.include_count; i++) {
        ASTNode *inc = role->data.role_decl.includes[i];
        ASTNode *included = find_role_decl_in_program(program,
            inc->data.include_stmt.role_name);
        ASTNode *method = find_role_operator_method(included, program, op, depth + 1);
        if (method != NULL)
            return method;
    }

    return NULL;
}

static Type *
type_check_role_operator_overload(ASTNode *expr, SemanticContext *ctx,
                                  Type *left, Type *right)
{
    if (ctx->program_root == NULL || left == NULL || left->name == NULL)
        return NULL;

    ASTNode *program = ctx->program_root;
    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_ROLE_DECL
            || stmt->data.role_decl.for_type == NULL
            || stmt->data.role_decl.for_type->type != AST_TYPE
            || stmt->data.role_decl.for_type->data.type.name == NULL
            || strcmp(stmt->data.role_decl.for_type->data.type.name, left->name) != 0) {
            continue;
        }

        ASTNode *method = find_role_operator_method(
            stmt, ctx->program_root, expr->data.binary.op.type, 0);
        if (method == NULL)
            continue;

        FuncParam *rhs_param = NULL;
        size_t rhs_param_count = 0;
        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (p != NULL && !(p->type == NULL && strcmp(p->name, "self") == 0)) {
                rhs_param = p;
                rhs_param_count++;
            }
        }
        if (rhs_param_count != 1)
            continue;

        Type *rhs_type = TYPE_INT;
        if (rhs_param != NULL && rhs_param->type != NULL)
            rhs_type = resolve_type_node(rhs_param->type, ctx);
        if (!type_is_assignable(right, rhs_type))
            continue;

        if (method->data.func_decl.return_type != NULL)
            return resolve_type_node(method->data.func_decl.return_type, ctx);
        return TYPE_VOID;
    }

    return NULL;
}

static Type *
type_check_operator_overload(ASTNode *expr, SemanticContext *ctx,
                             Type *left, Type *right)
{
    const char *suffix = operator_overload_suffix(expr->data.binary.op.type);
    if (suffix == NULL || left == NULL || right == NULL || left->name == NULL)
        return NULL;

    char fn_name[256];
    snprintf(fn_name, sizeof(fn_name), "operator_%s_%s", suffix, left->name);

    Symbol *sym = scope_lookup(ctx->scope, fn_name);
    if (sym == NULL || sym->kind != SYMBOL_FUNCTION
        || sym->type == NULL || sym->type->kind != TYPE_KIND_FUNCTION)
        return NULL;

    if (sym->type->data.function.param_count != 2)
        return NULL;

    Type *lhs = sym->type->data.function.param_types[0];
    Type *rhs = sym->type->data.function.param_types[1];
    if (!type_is_assignable(left, lhs) || !type_is_assignable(right, rhs))
        return NULL;

    sym->is_used = true;
    return sym->type->data.function.return_type;
}

static Type *
type_check_array_literal(ASTNode *expr, SemanticContext *ctx)
{
    if (expr->data.array_literal.count == 0)
        return wrap_constructed(TYPE_ARRAY, TYPE_INT);

    Type *elem_type = type_check_expression(expr->data.array_literal.elements[0], ctx);
    if (elem_type == NULL)
        elem_type = TYPE_UNKNOWN;

    for (size_t i = 1; i < expr->data.array_literal.count; i++) {
        Type *next = type_check_expression(expr->data.array_literal.elements[i], ctx);
        if (!type_is_assignable(next, elem_type) && !type_is_assignable(elem_type, next)) {
            semantic_error(ctx, expr->data.array_literal.elements[i],
                "Array literal element type mismatch: expected '%s', got '%s'",
                elem_type->name, next->name);
            elem_type = TYPE_UNKNOWN;
        }
    }

    return wrap_constructed(TYPE_ARRAY, elem_type);
}

static bool
check_call_arity(ASTNode *expr, size_t expected, const char *name,
                 SemanticContext *ctx)
{
    if (expr->data.call.arg_count != expected) {
        semantic_error(ctx, expr,
            "'%s' expects %zu argument(s), got %zu",
            name, expected, expr->data.call.arg_count);
        return false;
    }
    return true;
}

static Type *
type_check_stdlib_call(ASTNode *expr, const char *name, SemanticContext *ctx)
{
    if (strcmp(name, "Abs") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        return type_check_expression(expr->data.call.arguments[0], ctx);
    }
    if (strcmp(name, "Min") == 0 || strcmp(name, "Max") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        Type *a = type_check_expression(expr->data.call.arguments[0], ctx);
        Type *b = type_check_expression(expr->data.call.arguments[1], ctx);
        require_assignable(b, a, expr->data.call.arguments[1], ctx);
        return a;
    }
    if (strcmp(name, "StringLength") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(
            type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "Contains") == 0 || strcmp(name, "StringContains") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_STRING, expr->data.call.arguments[1], ctx);
        return TYPE_BOOL;
    }
    if (strcmp(name, "Replace") == 0 || strcmp(name, "StringReplace") == 0) {
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        for (size_t i = 0; i < 3; i++) {
            require_assignable(type_check_expression(expr->data.call.arguments[i], ctx),
                TYPE_STRING, expr->data.call.arguments[i], ctx);
        }
        return TYPE_STRING;
    }
    if (strcmp(name, "Substring") == 0) {
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_INT, expr->data.call.arguments[1], ctx);
        require_assignable(type_check_expression(expr->data.call.arguments[2], ctx),
            TYPE_INT, expr->data.call.arguments[2], ctx);
        return TYPE_STRING;
    }
    if (strcmp(name, "Trim") == 0 || strcmp(name, "StringTrim") == 0
        || strcmp(name, "Upper") == 0 || strcmp(name, "ToUpper") == 0
        || strcmp(name, "Lower") == 0 || strcmp(name, "ToLower") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        return TYPE_STRING;
    }
    if (strcmp(name, "Concat") == 0 || strcmp(name, "StringConcat") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_STRING, expr->data.call.arguments[1], ctx);
        return TYPE_STRING;
    }
    if (strcmp(name, "ArrayLength") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        Type *arg = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(arg, "Array")
            && !type_is_constructed_named(arg, "Slice")) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "ArrayLength requires Array<T> or Slice<T>, got '%s'", arg->name);
        }
        return TYPE_INT;
    }
    if (strcmp(name, "ToString") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_STRING;
    }
    if (strcmp(name, "Print") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        return TYPE_VOID;
    }

    /* Quantum resource builtins */
    if (strcmp(name, "ClaimQubit") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        return TYPE_QUBIT;
    }
    if (strcmp(name, "Measure") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "Entangle") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        require_assignable(type_check_qubit_use(expr->data.call.arguments[1], ctx),
            TYPE_QUBIT, expr->data.call.arguments[1], ctx);
        return TYPE_VOID;
    }
    if (strcmp(name, "QubitState") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "IsCollapsed") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    }
    if (strcmp(name, "ReleaseQubit") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        consume_qubit_value(expr->data.call.arguments[0], ctx, "released");
        return TYPE_VOID;
    }

    return NULL;
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

    /* I/O built-ins */
    case BUILTIN_FILE_OPEN:
        if (check_call_arity(call, 2, "FileOpen", ctx)) {
            require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                TYPE_STRING, call->data.call.arguments[0], ctx);
            require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
                TYPE_STRING, call->data.call.arguments[1], ctx);
        }
        return TYPE_INT;
    case BUILTIN_FILE_READ:
        if (check_call_arity(call, 1, "FileRead", ctx)) {
            require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                TYPE_INT, call->data.call.arguments[0], ctx);
        }
        return TYPE_STRING;
    case BUILTIN_FILE_WRITE:
        if (check_call_arity(call, 2, "FileWrite", ctx)) {
            require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                TYPE_INT, call->data.call.arguments[0], ctx);
            require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
                TYPE_STRING, call->data.call.arguments[1], ctx);
        }
        return TYPE_VOID;
    case BUILTIN_FILE_CLOSE:
        if (check_call_arity(call, 1, "FileClose", ctx)) {
            require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                TYPE_INT, call->data.call.arguments[0], ctx);
        }
        return TYPE_VOID;
    case BUILTIN_READ_FILE:
        if (check_call_arity(call, 1, "ReadFile", ctx)) {
            require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                TYPE_STRING, call->data.call.arguments[0], ctx);
        }
        return TYPE_STRING;
    case BUILTIN_WRITE_FILE:
        if (check_call_arity(call, 2, "WriteFile", ctx)) {
            require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                TYPE_STRING, call->data.call.arguments[0], ctx);
            require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
                TYPE_STRING, call->data.call.arguments[1], ctx);
        }
        return TYPE_VOID;
    case BUILTIN_INPUT:
        if (call->data.call.arg_count > 1) {
            semantic_error(ctx, call,
                "'Input' expects at most 1 argument, got %zu",
                call->data.call.arg_count);
            return TYPE_STRING;
        }
        if (call->data.call.arg_count == 1) {
            require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                TYPE_STRING, call->data.call.arguments[0], ctx);
        }
        return TYPE_STRING;
    case BUILTIN_PRINT:
        return type_check_stdlib_call(call, "Print", ctx);

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
        if (type_is_qubit(sym->type) && sym->is_consumed) {
            semantic_error(ctx, expr,
                "QubitSlot '%s' was moved or released and cannot be used again",
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

    case AST_ARRAY_LITERAL:
        return type_check_array_literal(expr, ctx);

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

    Type *overloaded = type_check_operator_overload(expr, ctx, left, right);
    if (overloaded == NULL)
        overloaded = type_check_role_operator_overload(expr, ctx, left, right);
    if (overloaded != NULL)
        return overloaded;

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
        {
            Type *stdlib_type = type_check_stdlib_call(expr, name, ctx);
            if (stdlib_type != NULL)
                return stdlib_type;
        }

        Symbol *sym = scope_lookup(ctx->scope, name);
        if (sym == NULL) {
            semantic_error(ctx, expr,
                "Undefined function '%s'", name);
            return TYPE_UNKNOWN;
        }
        /* Allow class/party/systemic/world constructors: TypeName() */
        if (sym->kind == SYMBOL_CLASS || sym->kind == SYMBOL_PARTY) {
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
            Type *param_type =
                sym->type->data.function.param_types[i];
            Type *arg_type = type_is_qubit(param_type)
                ? type_check_qubit_use(expr->data.call.arguments[i], ctx)
                : type_check_expression(expr->data.call.arguments[i], ctx);
            if (type_is_qubit(arg_type) || type_is_qubit(param_type)) {
                if (!type_is_qubit(arg_type) || !type_is_qubit(param_type)) {
                    semantic_error(ctx, expr->data.call.arguments[i],
                        "QubitSlot argument type mismatch");
                    continue;
                }
                if (expr->data.call.arguments[i]->type != AST_IDENTIFIER) {
                    semantic_error(ctx, expr->data.call.arguments[i],
                        "QubitSlot arguments must be moved from a named variable");
                    continue;
                }
                consume_qubit_value(expr->data.call.arguments[i], ctx, "moved");
                continue;
            }
            require_assignable(arg_type, param_type,
                               expr->data.call.arguments[i], ctx);
        }

        return sym->type->data.function.return_type;
    }

    /* Callee is a member access (method call) */
    if (callee->type == AST_MEMBER_ACCESS) {
        if (!(callee->data.member.object != NULL
              && callee->data.member.object->type == AST_IDENTIFIER
              && callee->data.member.object->data.identifier.name != NULL
              && callee->data.member.object->data.identifier.name[0] >= 'A'
              && callee->data.member.object->data.identifier.name[0] <= 'Z')) {
            /* Resolve object type for normal method calls.
             * Namespace/static-style calls like Math.Add are lowered later. */
            type_check_expression(callee->data.member.object, ctx);
        }
        return TYPE_UNKNOWN;
    }

    return TYPE_UNKNOWN;
}

Type *
type_check_member_access(ASTNode *expr, SemanticContext *ctx)
{
    if (expr->data.member.object != NULL
        && expr->data.member.object->type == AST_IDENTIFIER
        && expr->data.member.object->data.identifier.name != NULL
        && expr->data.member.object->data.identifier.name[0] >= 'A'
        && expr->data.member.object->data.identifier.name[0] <= 'Z') {
        /* Namespace / enum / static-style access such as Math.Add or Color.Red.
         * These are lowered later by the driver/codegen into flat symbols. */
        return TYPE_UNKNOWN;
    }

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

    if (type_is_qubit(target_type) || type_is_qubit(value_type)) {
        semantic_error(ctx, expr,
            "QubitSlot assignment is not allowed; quantum handles cannot be copied or rebound");
        return target_type;
    }

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

    if (type_is_qubit(decl_type)) {
        bool valid_qubit_init = false;
        if (init_type == TYPE_UNKNOWN) {
            valid_qubit_init = true;
        } else if (expr_is_qubit_claim(init)) {
            valid_qubit_init = true;
        } else if (init != NULL && init_type != NULL && type_is_qubit(init_type)) {
            if (init->type == AST_IDENTIFIER) {
                valid_qubit_init = true;
                consume_qubit_value(init, ctx, "moved");
            } else if (init->type == AST_CALL) {
                valid_qubit_init = true;
            }
        }
        if (!valid_qubit_init) {
            semantic_error(ctx, node,
                "QubitSlot values must come from ClaimQubit() or a moved QubitSlot value");
        }
    }

    if (decl_type != NULL && decl_type->kind == TYPE_KIND_SLOT) {
        Symbol *sym = symbol_create_slot(name, decl_type,
            decl_type->data.slot.is_secure, NULL, node->line, node->column);
        scope_declare(ctx->scope, sym);
        scope_register_slot(ctx->scope, sym);
        return !ctx->has_error;
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

    if (node->data.return_stmt.value != NULL
        && type_is_qubit(ret_type)
        && node->data.return_stmt.value->type == AST_IDENTIFIER) {
        consume_qubit_value(node->data.return_stmt.value, ctx, "returned");
    }

    return !ctx->has_error;
}

static FlowFlags
type_check_block_flow(ASTNode *node, SemanticContext *ctx, LoopFlowState *loop_flow)
{
    if (node == NULL)
        return FLOW_FALLTHROUGH;

    if (node->type != AST_BLOCK)
        return type_check_statement_flow(node, ctx, loop_flow);

    FlowFlags flags = FLOW_FALLTHROUGH;
    for (size_t i = 0; i < node->data.block.count; i++) {
        if ((flags & FLOW_FALLTHROUGH) == 0)
            break;

        FlowFlags stmt_flags =
            type_check_statement_flow(node->data.block.statements[i], ctx, loop_flow);

        flags &= ~FLOW_FALLTHROUGH;
        flags |= (stmt_flags & (FLOW_FALLTHROUGH
                              | FLOW_BREAK
                              | FLOW_CONTINUE
                              | FLOW_RETURN));
    }

    return flags;
}

static FlowFlags
type_check_if_stmt_flow(ASTNode *node, SemanticContext *ctx, LoopFlowState *loop_flow)
{
    Type *cond = type_check_expression(node->data.if_stmt.condition, ctx);
    QubitConsumeSnapshot base = snapshot_qubit_states(ctx);
    QubitConsumeSnapshot fallthrough = {0};
    bool has_fallthrough = false;
    FlowFlags flags = FLOW_NONE;
    FlowFlags then_flags = FLOW_NONE;

    if (!type_equals(cond, TYPE_BOOL)) {
        semantic_error(ctx, node,
            "If condition must be Bool, got '%s'", cond->name);
    }

    restore_qubit_states(&base);
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    then_flags = type_check_block_flow(node->data.if_stmt.then_branch, ctx, loop_flow);
    scope_exit(&ctx->scope);
    flags |= (then_flags & (FLOW_BREAK | FLOW_CONTINUE | FLOW_RETURN));
    if (then_flags & FLOW_FALLTHROUGH) {
        QubitConsumeSnapshot then_snap = snapshot_qubit_states(ctx);
        merge_qubit_snapshots_or(&fallthrough, &has_fallthrough, &then_snap);
        destroy_qubit_snapshot(&then_snap);
        flags |= FLOW_FALLTHROUGH;
    }

    if (node->data.if_stmt.else_branch != NULL) {
        FlowFlags else_flags = FLOW_NONE;
        restore_qubit_states(&base);
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        else_flags =
            type_check_statement_flow(node->data.if_stmt.else_branch, ctx, loop_flow);
        scope_exit(&ctx->scope);
        flags |= (else_flags & (FLOW_BREAK | FLOW_CONTINUE | FLOW_RETURN));
        if (else_flags & FLOW_FALLTHROUGH) {
            QubitConsumeSnapshot else_snap = snapshot_qubit_states(ctx);
            merge_qubit_snapshots_or(&fallthrough, &has_fallthrough, &else_snap);
            destroy_qubit_snapshot(&else_snap);
            flags |= FLOW_FALLTHROUGH;
        }
    } else {
        merge_qubit_snapshots_or(&fallthrough, &has_fallthrough, &base);
        flags |= FLOW_FALLTHROUGH;
    }

    if (has_fallthrough)
        restore_qubit_states(&fallthrough);
    else
        restore_qubit_states(&base);

    destroy_qubit_snapshot(&base);
    destroy_qubit_snapshot(&fallthrough);
    return flags;
}

static FlowFlags
type_check_match_stmt_flow(ASTNode *node, SemanticContext *ctx, LoopFlowState *loop_flow)
{
    Type *subj_type = type_check_expression(node->data.match_stmt.subject, ctx);
    QubitConsumeSnapshot base = snapshot_qubit_states(ctx);
    QubitConsumeSnapshot fallthrough = {0};
    bool has_fallthrough = false;
    FlowFlags flags = FLOW_NONE;

    for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
        ASTNode *mc = node->data.match_stmt.cases[i];

        restore_qubit_states(&base);
        scope_enter(&ctx->scope, SCOPE_BLOCK);

        if (mc->data.match_case.pattern != NULL) {
            Type *pat_type = type_check_expression(mc->data.match_case.pattern, ctx);
            if (!type_is_assignable(pat_type, subj_type) &&
                !type_is_assignable(subj_type, pat_type)) {
                semantic_error(ctx, mc->data.match_case.pattern,
                    "Case pattern type '%s' incompatible with match subject '%s'",
                    pat_type->name, subj_type->name);
            }
        }

        if (mc->data.match_case.guard != NULL) {
            Type *guard_type = type_check_expression(mc->data.match_case.guard, ctx);
            if (!type_equals(guard_type, TYPE_BOOL)) {
                semantic_error(ctx, mc->data.match_case.guard,
                    "Case guard must be Bool, got '%s'", guard_type->name);
            }
        }

        FlowFlags case_flags =
            type_check_block_flow(mc->data.match_case.body, ctx, loop_flow);
        scope_exit(&ctx->scope);
        flags |= (case_flags & (FLOW_BREAK | FLOW_CONTINUE | FLOW_RETURN));
        if (case_flags & FLOW_FALLTHROUGH) {
            QubitConsumeSnapshot case_snap = snapshot_qubit_states(ctx);
            merge_qubit_snapshots_or(&fallthrough, &has_fallthrough, &case_snap);
            destroy_qubit_snapshot(&case_snap);
            flags |= FLOW_FALLTHROUGH;
        }
    }

    if (node->data.match_stmt.default_body != NULL) {
        FlowFlags default_flags = FLOW_NONE;
        restore_qubit_states(&base);
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        default_flags =
            type_check_block_flow(node->data.match_stmt.default_body, ctx, loop_flow);
        scope_exit(&ctx->scope);
        flags |= (default_flags & (FLOW_BREAK | FLOW_CONTINUE | FLOW_RETURN));
        if (default_flags & FLOW_FALLTHROUGH) {
            QubitConsumeSnapshot default_snap = snapshot_qubit_states(ctx);
            merge_qubit_snapshots_or(&fallthrough, &has_fallthrough, &default_snap);
            destroy_qubit_snapshot(&default_snap);
            flags |= FLOW_FALLTHROUGH;
        }
    } else {
        merge_qubit_snapshots_or(&fallthrough, &has_fallthrough, &base);
        flags |= FLOW_FALLTHROUGH;
    }

    if (has_fallthrough)
        restore_qubit_states(&fallthrough);
    else
        restore_qubit_states(&base);

    destroy_qubit_snapshot(&base);
    destroy_qubit_snapshot(&fallthrough);
    return flags;
}

static FlowFlags
type_check_with_stmt_flow(ASTNode *node, SemanticContext *ctx, LoopFlowState *loop_flow)
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

    FlowFlags flags =
        type_check_block_flow(node->data.with_stmt.body, ctx, loop_flow);

    scope_auto_release_slots(ctx->scope);
    scope_exit(&ctx->scope);
    return flags;
}

static FlowFlags
type_check_statement_flow(ASTNode *node, SemanticContext *ctx, LoopFlowState *loop_flow)
{
    if (node == NULL)
        return FLOW_FALLTHROUGH;

    switch (node->type) {
    case AST_BLOCK:
        return type_check_block_flow(node, ctx, loop_flow);
    case AST_IF_STMT:
        return type_check_if_stmt_flow(node, ctx, loop_flow);
    case AST_MATCH_STMT:
        return type_check_match_stmt_flow(node, ctx, loop_flow);
    case AST_WITH_STMT:
        return type_check_with_stmt_flow(node, ctx, loop_flow);
    case AST_UNSAFE_BLOCK:
        if (node->data.unsafe_block.body != NULL)
            return type_check_block_flow(node->data.unsafe_block.body, ctx, loop_flow);
        return FLOW_FALLTHROUGH;
    case AST_DEFER_STMT:
        if (node->data.defer_stmt.body != NULL)
            return type_check_block_flow(node->data.defer_stmt.body, ctx, loop_flow);
        return FLOW_FALLTHROUGH;
    case AST_RETURN:
        type_check_return_stmt(node, ctx);
        return FLOW_RETURN;
    case AST_BREAK:
        if (ctx->loop_depth <= 0) {
            semantic_error(ctx, node, "'break' used outside of loop");
            return FLOW_NONE;
        }
        {
            QubitConsumeSnapshot snap = snapshot_qubit_states(ctx);
            loop_flow_record(loop_flow, true, &snap);
            destroy_qubit_snapshot(&snap);
        }
        return FLOW_BREAK;
    case AST_CONTINUE:
        if (ctx->loop_depth <= 0) {
            semantic_error(ctx, node, "'continue' used outside of loop");
            return FLOW_NONE;
        }
        {
            QubitConsumeSnapshot snap = snapshot_qubit_states(ctx);
            loop_flow_record(loop_flow, false, &snap);
            destroy_qubit_snapshot(&snap);
        }
        return FLOW_CONTINUE;
    default:
        type_check_statement(node, ctx);
        return FLOW_FALLTHROUGH;
    }
}

bool
type_check_if_stmt(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_if_stmt_flow(node, ctx, NULL);
    return !ctx->has_error;
}

bool
type_check_for_loop(ASTNode *node, SemanticContext *ctx)
{
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    ctx->loop_depth++;

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

    QubitConsumeSnapshot base = snapshot_qubit_states(ctx);
    QubitConsumeSnapshot merged = copy_qubit_snapshot(&base);
    QubitConsumeSnapshot entry = copy_qubit_snapshot(&base);
    bool known_iterations = false;
    size_t known_cap = for_loop_known_iteration_cap(node, &known_iterations);
    size_t max_iterations = (known_iterations && known_cap <= 1)
        ? 1
        : (base.count + 1);
    if (max_iterations == 0)
        max_iterations = 1;

    for (size_t iter = 0; iter < max_iterations; iter++) {
        LoopFlowState loop_flow = {0};
        QubitConsumeSnapshot backedge = {0};
        bool has_backedge = false;

        restore_qubit_states(&entry);
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        {
            FlowFlags body_flags =
                type_check_block_flow(node->data.for_loop.body, ctx, &loop_flow);
            if (body_flags & FLOW_FALLTHROUGH) {
                QubitConsumeSnapshot body_snap = snapshot_qubit_states(ctx);
                merge_qubit_states_or(&merged, &body_snap);
                merge_qubit_snapshots_or(&backedge, &has_backedge, &body_snap);
                destroy_qubit_snapshot(&body_snap);
            }
        }
        scope_exit(&ctx->scope);

        if (loop_flow.has_continue_states)
            merge_qubit_snapshots_or(&backedge, &has_backedge,
                                     &loop_flow.continue_states);
        if (loop_flow.has_break_states)
            merge_qubit_states_or(&merged, &loop_flow.break_states);

        destroy_loop_flow_state(&loop_flow);

        if (!has_backedge) {
            destroy_qubit_snapshot(&backedge);
            break;
        }

        if (qubit_snapshots_equal(&entry, &backedge)) {
            destroy_qubit_snapshot(&entry);
            entry = backedge;
            break;
        }

        destroy_qubit_snapshot(&entry);
        entry = backedge;
    }

    ctx->loop_depth--;
    scope_exit(&ctx->scope);

    restore_qubit_states(&merged);
    destroy_qubit_snapshot(&base);
    destroy_qubit_snapshot(&merged);
    destroy_qubit_snapshot(&entry);
    return !ctx->has_error;
}

bool
type_check_while_loop(ASTNode *node, SemanticContext *ctx)
{
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    ctx->loop_depth++;

    QubitConsumeSnapshot base = snapshot_qubit_states(ctx);
    QubitConsumeSnapshot merged = copy_qubit_snapshot(&base);
    QubitConsumeSnapshot entry = copy_qubit_snapshot(&base);
    size_t max_iterations = base.count + 1;
    if (max_iterations == 0)
        max_iterations = 1;

    for (size_t iter = 0; iter < max_iterations; iter++) {
        LoopFlowState loop_flow = {0};
        QubitConsumeSnapshot backedge = {0};
        bool has_backedge = false;

        restore_qubit_states(&entry);
        Type *cond = type_check_expression(node->data.while_loop.condition, ctx);
        if (!type_equals(cond, TYPE_BOOL)) {
            semantic_error(ctx, node,
                "While condition must be Bool, got '%s'", cond->name);
        }

        scope_enter(&ctx->scope, SCOPE_BLOCK);
        {
            FlowFlags body_flags =
                type_check_block_flow(node->data.while_loop.body, ctx, &loop_flow);
            if (body_flags & FLOW_FALLTHROUGH) {
                QubitConsumeSnapshot body_snap = snapshot_qubit_states(ctx);
                merge_qubit_states_or(&merged, &body_snap);
                merge_qubit_snapshots_or(&backedge, &has_backedge, &body_snap);
                destroy_qubit_snapshot(&body_snap);
            }
        }
        scope_exit(&ctx->scope);

        if (loop_flow.has_continue_states)
            merge_qubit_snapshots_or(&backedge, &has_backedge,
                                     &loop_flow.continue_states);
        if (loop_flow.has_break_states)
            merge_qubit_states_or(&merged, &loop_flow.break_states);

        destroy_loop_flow_state(&loop_flow);

        if (!has_backedge) {
            destroy_qubit_snapshot(&backedge);
            break;
        }

        if (qubit_snapshots_equal(&entry, &backedge)) {
            destroy_qubit_snapshot(&entry);
            entry = backedge;
            break;
        }

        destroy_qubit_snapshot(&entry);
        entry = backedge;
    }

    ctx->loop_depth--;
    scope_exit(&ctx->scope);

    restore_qubit_states(&merged);
    destroy_qubit_snapshot(&base);
    destroy_qubit_snapshot(&merged);
    destroy_qubit_snapshot(&entry);
    return !ctx->has_error;
}

bool
type_check_match_stmt(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_match_stmt_flow(node, ctx, NULL);
    return !ctx->has_error;
}

bool
type_check_with_stmt(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_with_stmt_flow(node, ctx, NULL);
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
    if (existing != NULL && existing->kind == SYMBOL_CLASS) {
        /* Forward-declared in Pass 1 — update kind */
        existing->kind = SYMBOL_PARTY;
        symbol_destroy(sym);
    } else if (existing != NULL) {
        semantic_error(ctx, node, "Redeclaration of party '%s'", name);
        symbol_destroy(sym);
        return false;
    } else {
        scope_declare(ctx->scope, sym);
    }

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
    case AST_BREAK:
        if (ctx->loop_depth <= 0) {
            semantic_error(ctx, node, "'break' used outside of loop");
            return false;
        }
        return true;
    case AST_CONTINUE:
        if (ctx->loop_depth <= 0) {
            semantic_error(ctx, node, "'continue' used outside of loop");
            return false;
        }
        return true;
    case AST_ENUM_DECL:
        return true;
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
    case AST_BIND_STMT:
        /* bind party.slot = Role; — validated at codegen level */
        return true;
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

    (void)type_check_block_flow(node, ctx, NULL);
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

    ctx->program_root = program;

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
        } else if (stmt->type == AST_ENUM_DECL) {
            const char *ename = stmt->data.enum_decl.name;
            if (ename != NULL && scope_lookup_current(ctx->scope, ename) == NULL) {
                Type *t = calloc(1, sizeof(Type));
                if (t != NULL) {
                    t->kind = TYPE_KIND_CLASS;
                    t->name = pergyra_strdup(ename);
                }
                Symbol *s = symbol_create_function(ename,
                    t != NULL ? t : TYPE_UNKNOWN, stmt->line, stmt->column);
                s->kind = SYMBOL_CLASS;
                scope_declare(ctx->scope, s);
            }
            Symbol *enum_sym = scope_lookup_current(ctx->scope, ename);
            for (size_t j = 0; j < stmt->data.enum_decl.variant_count; j++) {
                const char *vname = stmt->data.enum_decl.variants[j];
                if (vname == NULL || scope_lookup_current(ctx->scope, vname) != NULL)
                    continue;
                Symbol *vs = symbol_create_variable(vname,
                    enum_sym != NULL ? enum_sym->type : TYPE_UNKNOWN,
                    stmt->line, stmt->column);
                scope_declare(ctx->scope, vs);
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
        } else if (stmt->type == AST_PARTY_DECL
                   || stmt->type == AST_SYSTEMIC_DECL
                   || stmt->type == AST_WORLD_DECL) {
            /* Register party/systemic/world as class-like symbols
             * so that PartyName() constructor syntax works */
            const char *dname = NULL;
            if (stmt->type == AST_PARTY_DECL)
                dname = stmt->data.party_decl.name;
            else if (stmt->type == AST_SYSTEMIC_DECL)
                dname = stmt->data.systemic_decl.name;
            else
                dname = stmt->data.world_decl.name;
            if (dname != NULL && scope_lookup_current(ctx->scope, dname) == NULL) {
                Type *t = calloc(1, sizeof(Type));
                if (t != NULL) {
                    t->kind = TYPE_KIND_CLASS;
                    t->name = pergyra_strdup(dname);
                }
                Symbol *s = symbol_create_function(dname,
                    t != NULL ? t : TYPE_UNKNOWN, stmt->line, stmt->column);
                s->kind = SYMBOL_CLASS;
                scope_declare(ctx->scope, s);
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
