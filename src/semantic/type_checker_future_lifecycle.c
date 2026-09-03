/*
 * Future/RemoteFuture structured-lifecycle owner.
 *
 * Semantic flow owns containment. Runtime task records and backends consume
 * the admitted program and must not invent implicit drain/cancel behavior.
 */

#include "diag_codes.h"
#include "type_checker_internal.h"

bool
semantic_type_is_future_handle(const Type *type)
{
    return type_is_constructed_named(type, "Future")
        || type_is_constructed_named(type, "RemoteFuture");
}

static void
semantic_future_report(Symbol *symbol, ASTNode *site, SemanticContext *ctx,
                       const char *boundary, const char *reason)
{
    if (ctx == NULL || symbol == NULL || symbol->future_lifecycle_reported)
        return;
    symbol->future_lifecycle_reported = true;
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_TASK_LIFECYCLE,
        PGY_CAUSE_TASK_LIFECYCLE,
        PGY_FIX_AWAIT_TASK_BEFORE_EXIT,
        site,
        "Task completion handle '%s' is not retired at %s.\n"
        "Reason:\n"
        "- %s\n"
        "- Cancel requests cooperative cancellation but does not join or free the handle\n"
        "- every normal path must await the handle or transfer it to an explicit own Future parameter\n"
        "Fix:\n"
        "- await %s before leaving this scope\n"
        "- after Cancel(%s), still await %s to complete the join",
        symbol->name != NULL ? symbol->name : "<future>",
        boundary != NULL ? boundary : "scope exit",
        reason != NULL ? reason : "the handle is still live",
        symbol->name != NULL ? symbol->name : "the future",
        symbol->name != NULL ? symbol->name : "the future",
        symbol->name != NULL ? symbol->name : "the future");
}

bool
semantic_future_admit_spawn(ASTNode *site, SemanticContext *ctx)
{
    SemanticSpawnHandleUse use;

    if (ctx == NULL)
        return false;
    use = ctx->spawn_handle_use;
    /* A direct owner site must not leak through the spawned call's children. */
    ctx->spawn_handle_use = SEMANTIC_SPAWN_HANDLE_UNOWNED;
    if (use != SEMANTIC_SPAWN_HANDLE_UNOWNED)
        return true;

    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_TASK_LIFECYCLE,
        PGY_CAUSE_TASK_LIFECYCLE,
        PGY_FIX_AWAIT_TASK_BEFORE_EXIT,
        site,
        "spawn produces a completion handle that has no owner.\n"
        "Reason:\n"
        "- beta structured spawn requires a direct let binding or immediate await\n"
        "- an unowned temporary can outlive the lexical scope without a join\n"
        "Fix:\n"
        "- bind it: let pending = spawn Worker(...)\n"
        "- or join it immediately: await spawn Worker(...)");
    return false;
}

void
semantic_future_initialize_binding(Symbol *symbol, ASTNode *initializer,
                                   ASTNode *binding_site,
                                   SemanticContext *ctx)
{
    if (symbol == NULL || !semantic_type_is_future_handle(symbol->type))
        return;

    symbol->future_lifecycle_state = PGY_FUTURE_LIFECYCLE_LIVE;
    if (symbol->is_mut_binding) {
        semantic_future_report(symbol, binding_site, ctx, "mutable binding",
            "a completion handle is affine; rebinding could drop the only live join obligation");
        return;
    }

    if (initializer != NULL && initializer->type == AST_IDENTIFIER) {
        Symbol *source = lookup_identifier_symbol(initializer, ctx);
        if (source == NULL || !semantic_type_is_future_handle(source->type))
            return;
        /* A plain let alias hides the ownership move. Keep transfer visible at
         * the callable boundary instead of inventing implicit move syntax. */
        symbol->future_lifecycle_state = PGY_FUTURE_LIFECYCLE_RETIRED;
        symbol->is_consumed = true;
        semantic_future_report(source, initializer, ctx,
            "Future alias binding",
            source->future_lifecycle_state == PGY_FUTURE_LIFECYCLE_DIVERGED
                ? "only some incoming paths still own a live handle"
                : "a completion handle cannot be rebound; transfer is explicit only through an own Future parameter");
    }
}

void
semantic_future_initialize_parameter(Symbol *symbol)
{
    if (symbol != NULL && semantic_type_is_future_handle(symbol->type)
        && symbol->param_mode == PARAM_MODE_OWN) {
        symbol->future_lifecycle_state = PGY_FUTURE_LIFECYCLE_LIVE;
    }
}

void
semantic_future_complete(Symbol *symbol)
{
    if (symbol == NULL || !semantic_type_is_future_handle(symbol->type))
        return;
    symbol->future_lifecycle_state = PGY_FUTURE_LIFECYCLE_RETIRED;
    symbol->is_consumed = true;
    symbol->is_used = true;
}

void
semantic_future_transfer_argument(ASTNode *argument, SemanticContext *ctx)
{
    Symbol *symbol = lookup_identifier_symbol(argument, ctx);
    if (symbol == NULL || !semantic_type_is_future_handle(symbol->type))
        return;
    if (symbol->future_lifecycle_state == PGY_FUTURE_LIFECYCLE_LIVE) {
        symbol->future_lifecycle_state = PGY_FUTURE_LIFECYCLE_RETIRED;
        symbol->is_consumed = true;
        symbol->is_used = true;
        return;
    }
    semantic_future_report(symbol, argument, ctx, "owned Future transfer",
        "the source handle is not live on every incoming path");
}

bool
semantic_future_validate_use(Symbol *symbol, ASTNode *site,
                             SemanticContext *ctx)
{
    if (symbol == NULL || !semantic_type_is_future_handle(symbol->type)
        || symbol->future_lifecycle_state != PGY_FUTURE_LIFECYCLE_DIVERGED) {
        return true;
    }
    semantic_future_report(symbol, site, ctx, "Future use",
        "alternative control-flow paths disagree about whether the handle was already joined or transferred");
    return false;
}

bool
semantic_future_use_is_invalid(ASTNode *expression, SemanticContext *ctx)
{
    Symbol *symbol = lookup_identifier_symbol(expression, ctx);

    if (symbol == NULL || !semantic_type_is_future_handle(symbol->type))
        return false;
    return symbol->future_lifecycle_state == PGY_FUTURE_LIFECYCLE_RETIRED
        || symbol->future_lifecycle_state == PGY_FUTURE_LIFECYCLE_DIVERGED;
}

bool
semantic_future_use_failure_was_reported(ASTNode *expression,
    size_t diagnostic_base, SemanticContext *ctx)
{
    return semantic_future_use_is_invalid(expression, ctx)
        || ctx->diagnostic_count > diagnostic_base;
}

bool
semantic_future_require_until(Scope *scope, Scope *stop_exclusive,
                              ASTNode *site, SemanticContext *ctx,
                              const char *boundary)
{
    bool ok = true;

    if (ctx != NULL && ctx->future_lifecycle_unreachable_depth > 0)
        return true;

    for (Scope *current = scope;
         current != NULL && current != stop_exclusive;
         current = current->parent) {
        for (size_t i = 0; i < current->symbol_count; i++) {
            Symbol *symbol = current->symbols[i];
            if (symbol == NULL || !semantic_type_is_future_handle(symbol->type))
                continue;
            if (symbol->future_lifecycle_state == PGY_FUTURE_LIFECYCLE_LIVE) {
                semantic_future_report(symbol, site, ctx, boundary,
                    "the handle is still live");
                ok = false;
            } else if (symbol->future_lifecycle_state
                       == PGY_FUTURE_LIFECYCLE_DIVERGED) {
                semantic_future_report(symbol, site, ctx, boundary,
                    "only some normal paths joined or transferred the handle");
                ok = false;
            }
        }
    }
    return ok;
}

bool
semantic_future_require_scope_retired(Scope *scope, ASTNode *site,
                                      SemanticContext *ctx,
                                      const char *boundary)
{
    return semantic_future_require_until(
        scope, scope != NULL ? scope->parent : NULL,
        site, ctx, boundary);
}

bool
semantic_future_require_function_retired(Scope *scope, ASTNode *site,
                                         SemanticContext *ctx,
                                         const char *boundary)
{
    Scope *function_scope = scope;

    while (function_scope != NULL
           && function_scope->kind != SCOPE_FUNCTION) {
        function_scope = function_scope->parent;
    }
    return semantic_future_require_until(
        scope,
        function_scope != NULL ? function_scope->parent : NULL,
        site, ctx, boundary);
}
