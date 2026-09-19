/*
 * Future/RemoteFuture structured-lifecycle owner.
 *
 * Semantic flow owns containment. Runtime task records and backends consume
 * the admitted program and must not invent implicit drain/cancel behavior.
 */

#include "diag_codes.h"
#include "type_checker_internal.h"
#include <stdlib.h>
#include <string.h>

bool
semantic_type_is_future_handle(const Type *type)
{
    return type_is_constructed_named(type, "Future")
        || type_is_constructed_named(type, "RemoteFuture");
}

/* Type arguments are not storage by themselves. Inspect only constructors
 * whose current value representation carries their arguments, so an unused
 * phantom parameter does not make an unrelated nominal type affine. */
static bool
future_constructor_name_stores_arguments(const char *name)
{
    static const char *const stored[] = {
        "Array", "List", "Queue", "Set", "HashMap", "Option", "Result",
        "Box", "Rc", "Channel"
    };
    if (name == NULL)
        return false;
    for (size_t i = 0; i < sizeof(stored) / sizeof(stored[0]); i++) {
        if (strcmp(name, stored[i]) == 0)
            return true;
    }
    return false;
}

static bool
future_type_constructor_stores_arguments(const Type *type)
{
    Type *constructor = type_constructed_constructor(type);
    return constructor != NULL
        && future_constructor_name_stores_arguments(constructor->name);
}

static bool future_type_contains_at_boundary(const Type *type,
    SemanticContext *ctx, unsigned depth);
static bool future_decl_fields_syntax_contain_handle(ASTNode *decl,
    const bool *affine_params, size_t param_count,
    SemanticContext *ctx, unsigned depth);

static void
future_storage_report_resolution_oom(SemanticContext *ctx, ASTNode *decl)
{
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_UNKNOWN_TYPE,
        PGY_CAUSE_RESOLUTION_OOM,
        PGY_FIX_REDUCE_SCOPE_OR_RETRY,
        decl,
        "Could not allocate nominal field facts for Future containment.\n"
        "Reason:\n"
        "- aggregate storage cannot be admitted without its field shape\n"
        "Fix:\n"
        "- reduce this compilation unit size and retry");
}

/* A generic argument is storage only where the declaration field actually
 * mentions it, including under a known stored wrapper. An unused Phantom<T>
 * argument is therefore not an affine stored handle. */
static bool
future_field_syntax_contains_handle(const ASTNode *field_type,
                                    const GenericParams *class_generics,
                                    const bool *affine_params,
                                    size_t param_count,
                                    SemanticContext *ctx,
                                    unsigned depth)
{
    const char *name;
    GenericParams *field_args;
    ASTNode *nested_decl;

    if (field_type == NULL || depth > 32)
        return depth > 32;
    if (ast_type_tuple_element_count(field_type) > 0) {
        for (size_t i = 0; i < ast_type_tuple_element_count(field_type); i++) {
            if (future_field_syntax_contains_handle(
                    ast_type_tuple_element(field_type, i), class_generics,
                    affine_params, param_count, ctx, depth + 1))
                return true;
        }
        return false;
    }
    name = ast_type_name(field_type);
    if (name == NULL)
        return false;
    if (strcmp(name, "Future") == 0
        || strcmp(name, "RemoteFuture") == 0)
        return true;
    for (size_t i = 0; i < ast_generic_param_count(class_generics); i++) {
        GenericParam *param = ast_generic_param_at(class_generics, i);
        const char *param_name = ast_generic_param_name(param);
        if (param_name != NULL && strcmp(name, param_name) == 0)
            return i < param_count ? affine_params[i] : true;
    }
    field_args = ast_type_generic_args(field_type);
    if (future_constructor_name_stores_arguments(name)) {
        for (size_t i = 0; i < ast_generic_param_count(field_args); i++) {
            if (future_field_syntax_contains_handle(
                    ast_generic_param_constraint(
                        ast_generic_param_at(field_args, i)),
                    class_generics, affine_params, param_count, ctx,
                    depth + 1))
                return true;
        }
        return false;
    }
    nested_decl = ctx != NULL
        ? semantic_find_class_decl_by_name(ctx, name) : NULL;
    if (nested_decl != NULL) {
        GenericParams *nested_generics = ast_class_generic_params(nested_decl);
        size_t nested_count = ast_generic_param_count(nested_generics);
        bool *nested_affine = nested_count > 0
            ? calloc(nested_count, sizeof(bool)) : NULL;
        bool contains;
        if (nested_count > 0 && nested_affine == NULL) {
            future_storage_report_resolution_oom(ctx, nested_decl);
            return false;
        }
        for (size_t i = 0; i < nested_count; i++) {
            ASTNode *arg = i < ast_generic_param_count(field_args)
                ? ast_generic_param_constraint(
                    ast_generic_param_at(field_args, i)) : NULL;
            if (arg == NULL)
                arg = ast_generic_param_default_type(
                    ast_generic_param_at(nested_generics, i));
            nested_affine[i] = arg == NULL
                || future_field_syntax_contains_handle(
                    arg, class_generics, affine_params, param_count,
                    ctx, depth + 1);
        }
        contains = future_decl_fields_syntax_contain_handle(
            nested_decl, nested_affine, nested_count, ctx, depth + 1);
        free(nested_affine);
        return contains;
    }
    /* If the class declaration is unavailable, an affine type argument must
     * not be assumed phantom at this storage boundary. */
    for (size_t i = 0; i < ast_generic_param_count(field_args); i++) {
        if (future_field_syntax_contains_handle(
                ast_generic_param_constraint(
                    ast_generic_param_at(field_args, i)),
                class_generics, affine_params, param_count,
                ctx, depth + 1))
            return true;
    }
    return false;
}

static bool
future_decl_fields_syntax_contain_handle(ASTNode *decl,
                                        const bool *affine_params,
                                        size_t param_count,
                                        SemanticContext *ctx,
                                        unsigned depth)
{
    PgyDeclField *fields = NULL;
    size_t count;
    bool contains = false;

    if (decl == NULL || decl->type != AST_CLASS_DECL)
        return false;
    if (!pgy_class_decl_field_model_try_build(decl, &fields, &count)) {
        future_storage_report_resolution_oom(ctx, decl);
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (future_field_syntax_contains_handle(
                fields[i].type_ast, ast_class_generic_params(decl),
                affine_params, param_count, ctx, depth + 1)) {
            contains = true;
            break;
        }
    }
    pgy_decl_field_model_free(fields, count);
    return contains;
}

static bool
future_nominal_fields_contain_handle(const Type *type,
                                    SemanticContext *ctx,
                                    unsigned depth)
{
    ASTNode *decl = semantic_host_decl_for_type(ctx, type);
    GenericParams *generics;
    size_t param_count;
    bool *affine_params;
    bool contains;

    if (decl == NULL || decl->type != AST_CLASS_DECL)
        return true;
    generics = ast_class_generic_params(decl);
    param_count = ast_generic_param_count(generics);
    affine_params = param_count > 0
        ? calloc(param_count, sizeof(bool)) : NULL;
    if (param_count > 0 && affine_params == NULL) {
        future_storage_report_resolution_oom(ctx, decl);
        return false;
    }
    for (size_t i = 0; i < param_count; i++) {
        Type *arg = type_constructed_arg(type, i);
        affine_params[i] = arg == NULL
            || future_type_contains_at_boundary(arg, ctx, depth + 1);
    }
    contains = future_decl_fields_syntax_contain_handle(
        decl, affine_params, param_count, ctx, depth + 1);
    free(affine_params);
    return contains;
}

/* This is only a cheap candidate filter, not a storage verdict. Phantom<T>
 * still reaches the field model when T mentions Future, then remains legal
 * if none of its fields physically carry T. */
static bool
future_type_argument_mentions_handle(const Type *type, unsigned depth)
{
    if (type == NULL)
        return false;
    if (depth > 32)
        return true;
    if (semantic_type_is_future_handle(type))
        return true;
    if (type->kind == TYPE_KIND_SLOT)
        return future_type_argument_mentions_handle(
            type_slot_inner_type(type), depth + 1);
    if (type->kind == TYPE_KIND_TUPLE) {
        for (size_t i = 0; i < type_tuple_arity(type); i++) {
            if (future_type_argument_mentions_handle(
                    type_tuple_get_element(type, i), depth + 1))
                return true;
        }
    }
    if (type->kind == TYPE_KIND_CONSTRUCTED) {
        for (size_t i = 0; i < type_constructed_arg_count(type); i++) {
            if (future_type_argument_mentions_handle(
                    type_constructed_arg(type, i), depth + 1))
                return true;
        }
    }
    return false;
}

static bool
future_type_contains_at_boundary(const Type *type,
                                 SemanticContext *ctx,
                                 unsigned depth)
{
    if (type == NULL)
        return false;
    if (depth > 32)
        return true;
    if (semantic_type_is_future_handle(type))
        return true;
    if (type->kind == TYPE_KIND_SLOT)
        return future_type_contains_at_boundary(
            type_slot_inner_type(type), ctx, depth + 1);
    if (type->kind == TYPE_KIND_TUPLE) {
        for (size_t i = 0; i < type_tuple_arity(type); i++) {
            if (future_type_contains_at_boundary(
                    type_tuple_get_element(type, i), ctx, depth + 1))
                return true;
        }
        return false;
    }
    if (type->kind == TYPE_KIND_CONSTRUCTED
        && future_type_constructor_stores_arguments(type)) {
        for (size_t i = 0; i < type_constructed_arg_count(type); i++) {
            if (future_type_contains_at_boundary(
                    type_constructed_arg(type, i), ctx, depth + 1))
                return true;
        }
        return false;
    }
    /* A concrete nominal field is refused at its declaration. A generic
     * instance needs field inspection only when an actual type argument can
     * carry a completion handle. */
    if (ctx != NULL && type->kind == TYPE_KIND_CONSTRUCTED
        && future_type_argument_mentions_handle(type, depth + 1))
        return future_nominal_fields_contain_handle(type, ctx, depth + 1);
    return false;
}

bool
semantic_future_reject_aggregate_storage(ASTNode *site,
                                         const Type *stored_type,
                                         SemanticContext *ctx,
                                         const char *boundary)
{
    if (ctx == NULL
        || !future_type_contains_at_boundary(stored_type, ctx, 0))
        return false;
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_TASK_LIFECYCLE,
        PGY_CAUSE_TASK_LIFECYCLE,
        PGY_FIX_AWAIT_TASK_BEFORE_EXIT,
        site,
        "Completion handle storage in %s is not admitted for type '%s'.\n"
        "Reason:\n"
        "- Future/RemoteFuture completion handles are affine join obligations\n"
        "- aggregate element copying or extraction could duplicate one live handle\n"
        "- no aggregate move/retirement plan is admitted at this boundary\n"
        "Fix:\n"
        "- keep the handle in its direct binding and await it once\n"
        "- transfer it only through an explicit own Future parameter",
        boundary != NULL ? boundary : "aggregate storage",
        type_name_or_unknown(stored_type));
    return true;
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
