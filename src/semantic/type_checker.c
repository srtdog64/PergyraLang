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
    d->message = strdup(buf);

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

Type *
resolve_type_node(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL)
        return TYPE_VOID;

    if (node->type != AST_TYPE && node->type != AST_GENERIC_TYPE)
        return TYPE_UNKNOWN;

    const char *name = node->data.type.name;

    /* Built-in primitive types */
    if (strcmp(name, "Int")    == 0) return TYPE_INT;
    if (strcmp(name, "Long")   == 0) return TYPE_LONG;
    if (strcmp(name, "Float")  == 0) return TYPE_FLOAT;
    if (strcmp(name, "Double") == 0) return TYPE_DOUBLE;
    if (strcmp(name, "Bool")   == 0) return TYPE_BOOL;
    if (strcmp(name, "String") == 0) return TYPE_STRING;
    if (strcmp(name, "Void")   == 0) return TYPE_VOID;

    /* Slot<T> */
    if (strcmp(name, "Slot") == 0) {
        if (node->data.type.generic_args == NULL
            || node->data.type.generic_args->count == 0) {
            semantic_error(ctx, node, "Slot requires a type argument");
            return TYPE_UNKNOWN;
        }
        /* parse_generic_params stores "String" in param->name,
           not in param->constraint (which is for T: Trait bounds). */
        GenericParam *gp = node->data.type.generic_args->params[0];
        ASTNode *inner_node = gp->constraint;
        if (inner_node == NULL)
            inner_node = ast_create_type(gp->name);
        Type *inner = resolve_type_node(inner_node, ctx);
        return type_create_slot(inner, false);
    }

    /* SecureSlot<T> */
    if (strcmp(name, "SecureSlot") == 0) {
        if (node->data.type.generic_args == NULL
            || node->data.type.generic_args->count == 0) {
            semantic_error(ctx, node, "SecureSlot requires a type argument");
            return TYPE_UNKNOWN;
        }
        GenericParam *gp = node->data.type.generic_args->params[0];
        ASTNode *inner_node = gp->constraint;
        if (inner_node == NULL)
            inner_node = ast_create_type(gp->name);
        Type *inner = resolve_type_node(inner_node, ctx);
        return type_create_slot(inner, true);
    }

    /* User-defined type: look up in scope */
    Symbol *sym = scope_lookup(ctx->scope, name);
    if (sym != NULL)
        return sym->type;

    semantic_error(ctx, node, "Unknown type '%s'", name);
    return TYPE_UNKNOWN;
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

    case AST_ASSIGNMENT:
        return type_check_assignment(expr, ctx);

    case AST_AWAIT_EXPR:
        if (!ctx->in_async_func) {
            semantic_error(ctx, expr,
                "'await' used outside of async function");
        }
        return type_check_expression(expr->data.await_expr.expression, ctx);

    default:
        return TYPE_UNKNOWN;
    }
}

Type *
type_check_binary(ASTNode *expr, SemanticContext *ctx)
{
    Type *left  = type_check_expression(expr->data.binary.left,  ctx);
    Type *right = type_check_expression(expr->data.binary.right, ctx);

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

        Symbol *sym = scope_lookup(ctx->scope, name);
        if (sym == NULL) {
            semantic_error(ctx, expr,
                "Undefined function '%s'", name);
            return TYPE_UNKNOWN;
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
    type_check_expression(expr->data.member.object, ctx);
    /* Full member resolution deferred to Phase 2 */
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

    /* Normal variable declaration */
    Type *init_type = (init != NULL)
                      ? type_check_expression(init, ctx)
                      : TYPE_VOID;

    Type *decl_type = (ann != NULL)
                      ? resolve_type_node(ann, ctx)
                      : init_type;

    if (ann != NULL && init != NULL)
        require_assignable(init_type, decl_type, init, ctx);

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
    case AST_IF_STMT:
        return type_check_if_stmt(node, ctx);
    case AST_FOR_LOOP:
        return type_check_for_loop(node, ctx);
    case AST_RETURN:
        return type_check_return_stmt(node, ctx);
    case AST_WITH_STMT:
        return type_check_with_stmt(node, ctx);
    case AST_PARALLEL_BLOCK:
        return type_check_parallel_block(node, ctx);
    case AST_BLOCK:
        return type_check_block(node, ctx);
    case AST_EXPRESSION_STMT:
        type_check_expression(node, ctx);
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

    /* Build parameter types for the function type */
    size_t   param_count = node->data.func_decl.param_count;
    Type   **param_types = NULL;

    if (param_count > 0) {
        param_types = calloc(param_count, sizeof(Type *));
        if (param_types == NULL)
            return false;
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

    /* Check body in new function scope */
    scope_enter(&ctx->scope, SCOPE_FUNCTION);

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
    class_type->name = strdup(name);

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
                Type *placeholder = type_create_function(NULL, 0, TYPE_VOID);
                Symbol *s = symbol_create_function(fname, placeholder,
                                                    stmt->line, stmt->column);
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
