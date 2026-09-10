#include "callable_capability_equations_internal.h"
#include "capability_analyze.h"
#include "type_checker_internal.h"
#include "diag_codes.h"

#include <stdlib.h>
#include <string.h>

void *
callable_capability_allocate_equation(SemanticContext *ctx, size_t bytes)
{
    void *result = pgy_arena_calloc(&ctx->scratch_arena, bytes);
    if (result == NULL) {
        if (ctx->callable_capabilities != NULL)
            ctx->callable_capabilities->failed = true;
        semantic_error(ctx, ctx->current_function_decl, "Could not allocate callable capability facts");
    }
    return result;
}

static CapabilityTarget
capability_symbol_target(Symbol *symbol)
{
    CapabilityTarget value = {0};
    if (symbol == NULL || symbol->type == NULL
        || symbol->type->kind != TYPE_KIND_FUNCTION || symbol->decl_syntax_id == 0)
        return value;
    value.id = symbol->decl_syntax_id;
    value.kind = symbol->kind == SYMBOL_ENUM_CONSTRUCTOR ? CAP_CONSTRUCTOR
        : symbol->kind == SYMBOL_FUNCTION || symbol->kind == SYMBOL_INTENT
        ? CAP_DECL : symbol->is_parameter ? CAP_FORMAL : CAP_BINDING;
    return value;
}

static CapabilityTarget
capability_expression_target(SemanticContext *ctx, ASTNode *expression)
{
    if (expression != NULL && expression->type == AST_IDENTIFIER)
        return capability_symbol_target(scope_lookup(ctx->scope, ast_identifier_name(expression)));
    if (expression != NULL && expression->type == AST_LAMBDA_EXPR)
        return (CapabilityTarget){CAP_DECL, ast_node_stable_id(expression)};
    if (expression != NULL && expression->type == AST_CALL)
        return (CapabilityTarget){CAP_RESULT, ast_node_stable_id(expression)};
    return (CapabilityTarget){0};
}

CallableCapabilityRoutine *
callable_capability_enter(SemanticContext *ctx, ASTNode *decl, Type **params, size_t count)
{
    CallableCapabilityRoutine *previous = ctx->current_callable_capability;
    if (ctx->callable_capabilities == NULL) {
        ctx->callable_capabilities = callable_capability_allocate_equation(ctx,
            sizeof(*ctx->callable_capabilities));
        if (ctx->callable_capabilities == NULL)
            return previous;
    }
    CallableCapabilityRoutine *routine = callable_capability_allocate_equation(ctx, sizeof(*routine));
    if (routine == NULL)
        return previous;
    routine->decl = decl;
    routine->id = ast_node_stable_id(decl);
    routine->declared_mask = decl->type == AST_FUNC_DECL
        ? ast_func_declared_capabilities(decl) : 0;
    routine->formals = callable_capability_allocate_equation(ctx, (count + 1) * sizeof(uint32_t));
    if (routine->formals == NULL)
        return previous;
    for (size_t i = 0; i < count; i++) {
        if (params[i] == NULL || params[i]->kind != TYPE_KIND_FUNCTION)
            continue;
        uint32_t id = 0;
        if (decl->type == AST_LAMBDA_EXPR)
            id = ast_node_stable_id(ast_lambda_param(decl, i));
        else if (decl->type == AST_EVENT_DECL)
            id = ast_node_stable_id(ast_event_param(decl, i));
        else if (decl->type == AST_INTENT_DECL) {
            size_t bindings = ast_intent_decl_binding_count(decl);
            size_t involves = ast_intent_decl_involve_count(decl);
            ASTNode *binding = bindings > 0
                ? ast_intent_decl_bindings(decl, NULL)[i]
                : i < involves ? ast_intent_decl_involves(decl, NULL)[i]
                : ast_intent_decl_values(decl, NULL)[i - involves];
            id = ast_node_stable_id(binding);
        }
        else if (decl->type == AST_FUNC_DECL)
            id = ast_func_param_stable_id(ast_func_param(decl, i));
        routine->formals[routine->count++] = id;
    }
    routine->next = ctx->callable_capabilities->routines;
    ctx->callable_capabilities->routines = routine;
    ctx->callable_capabilities->count++;
    ctx->callable_capabilities->equation_count += count + 1;
    ctx->current_callable_capability = routine;
    return previous;
}

void
callable_capability_program_begin(SemanticContext *ctx, ASTNode *program)
{
    (void)callable_capability_enter(ctx, program, NULL, 0);
}

void
callable_capability_set_effect_contract(SemanticContext *ctx,
    uint32_t declared_effects, bool has_contract)
{
    CallableCapabilityRoutine *routine = ctx->current_callable_capability;
    if (routine == NULL) return;
    routine->declared_effects = declared_effects;
    routine->has_effect_contract = has_contract;
}

void
callable_capability_leave(SemanticContext *ctx,
    CallableCapabilityRoutine *previous, Type *type, uint32_t direct_mask,
    uint32_t direct_effects)
{
    CallableCapabilityRoutine *routine = ctx->current_callable_capability;
    if (routine != NULL && routine != previous) {
        routine->type = type;
        routine->direct_mask = direct_mask;
        routine->direct_effects = direct_effects;
        type_function_set_capabilities(type, direct_mask | routine->declared_mask);
        /* Existing branch/parallel diagnostics may observe the partial type.
         * It never feeds the fixed point: only direct effects and call edges do. */
        type_function_set_effects(type,
            type_effect_mask_join(ctx->current_function_effects, routine->declared_effects));
        /* Fragment checking has no program seal and publishes no executable.
         * Preserve its direct contract diagnostics through the same policy. */
        if (ctx->program_root == NULL && routine->decl->type == AST_FUNC_DECL)
            type_check_function_effect_contract(routine->decl, ctx,
                routine->declared_effects, routine->has_effect_contract,
                ctx->current_function_effects);
    }
    ctx->current_callable_capability = previous;
}

void
callable_capability_record_binding(SemanticContext *ctx,
                                   Symbol *symbol, ASTNode *value)
{
    if (symbol == NULL || symbol->type == NULL
        || symbol->type->kind != TYPE_KIND_FUNCTION
        || ctx->callable_capabilities == NULL)
        return;
    CapabilityBinding *binding = callable_capability_allocate_equation(ctx, sizeof(*binding));
    if (binding == NULL)
        return;
    binding->id = symbol->decl_syntax_id;
    binding->value = capability_expression_target(ctx, value);
    binding->next = ctx->callable_capabilities->bindings;
    ctx->callable_capabilities->bindings = binding;
    ctx->callable_capabilities->equation_count++;
}

void
callable_capability_record_return(SemanticContext *ctx, ASTNode *value)
{
    CallableCapabilityRoutine *routine = ctx->current_callable_capability;
    if (routine == NULL) return;
    CapabilityTarget target = capability_expression_target(ctx, value);
    if (!routine->has_result) routine->result = target;
    else if (target.kind != routine->result.kind || target.id != routine->result.id)
        routine->result = (CapabilityTarget){0};
    if (target.kind == CAP_UNKNOWN) {
        routine->result_unknown = true;
    } else {
        CapabilityResultTarget *seen = routine->results;
        while (seen != NULL && (seen->target.kind != target.kind
               || seen->target.id != target.id))
            seen = seen->next;
        if (seen == NULL) {
            CapabilityResultTarget *entry =
                callable_capability_allocate_equation(ctx, sizeof(*entry));
            if (entry == NULL) return;
            entry->target = target;
            entry->next = routine->results;
            routine->results = entry;
        }
    }
    routine->has_result = true;
}

void
callable_capability_invalidate_binding(SemanticContext *ctx, Symbol *symbol)
{
    if (ctx->callable_capabilities == NULL || symbol == NULL)
        return;
    for (CapabilityBinding *b = ctx->callable_capabilities->bindings; b; b = b->next)
        if (b->id == symbol->decl_syntax_id)
            b->value = (CapabilityTarget){0};
}

void
callable_capability_record_call(SemanticContext *ctx, ASTNode *site,
                               Symbol *callee, Type **actual_types)
{
    CallableCapabilityRoutine *routine = ctx->current_callable_capability;
    if (routine == NULL) {
        /* A standalone expression query has no executable routine boundary. */
        if (callee != NULL)
            semantic_record_capability(ctx, type_function_capabilities(callee->type));
        return;
    }
    CapabilityCall *call = callable_capability_allocate_equation(ctx, sizeof(*call));
    bool event_invoke = site->type == AST_EVENT_INVOKE;
    size_t count = event_invoke ? ast_event_invoke_arg_count(site)
                               : ast_call_arg_count(site);
    if (call == NULL)
        return;
    call->site = site;
    call->callee = capability_symbol_target(callee);
    call->actuals = callable_capability_allocate_equation(ctx, (count + 1) * sizeof(CapabilityTarget));
    if (call->actuals == NULL)
        return;
    for (size_t i = 0; i < count; i++) {
        Type *param = type_function_param_type(callee != NULL ? callee->type : NULL, i);
        if (param == NULL || param->kind != TYPE_KIND_FUNCTION)
            continue;
        call->actuals[call->count++] = actual_types == NULL
            || (actual_types[i] != NULL && actual_types[i]->kind == TYPE_KIND_FUNCTION)
            ? capability_expression_target(ctx, event_invoke
                ? ast_event_invoke_argument(site, i) : ast_call_argument(site, i))
            : (CapabilityTarget){0};
    }
    call->next = routine->calls;
    routine->calls = call;
    ctx->callable_capabilities->equation_count++;
}

void
callable_capability_record_method_call(SemanticContext *ctx, ASTNode *site,
                                       Type **params, Type **actual_types)
{
    CallableCapabilityRoutine *routine = ctx->current_callable_capability;
    if (routine == NULL) return;
    CapabilityCall *call = callable_capability_allocate_equation(ctx, sizeof(*call));
    size_t count = ast_call_arg_count(site);
    if (call == NULL) return;
    call->site = site;
    call->callee = (CapabilityTarget){CAP_DECL,
        ast_call_semantic_callee_decl_id(site)};
    call->actuals = callable_capability_allocate_equation(ctx, (count + 1) * sizeof(CapabilityTarget));
    if (call->actuals == NULL) return;
    for (size_t p = 0; p < count; p++)
        if (params[p] != NULL && params[p]->kind == TYPE_KIND_FUNCTION)
            call->actuals[call->count++] = actual_types[p] != NULL
                ? capability_expression_target(ctx, ast_call_argument(site, p))
                : (CapabilityTarget){0};
    call->next = routine->calls;
    routine->calls = call;
    ctx->callable_capabilities->equation_count++;
}

void
callable_capability_record_subscription(SemanticContext *ctx,
                                        ASTNode *event, ASTNode *handler)
{
    if (ctx->callable_capabilities == NULL) return;
    CapabilityTarget target = capability_expression_target(ctx, event);
    CapabilityCall *call = callable_capability_allocate_equation(ctx, sizeof(*call));
    if (call == NULL) return;
    call->site = handler;
    call->event_id = target.kind == CAP_DECL ? target.id : 0;
    call->callee = capability_expression_target(ctx, handler);
    call->next = ctx->callable_capabilities->subscriptions;
    ctx->callable_capabilities->subscriptions = call;
    ctx->callable_capabilities->equation_count++;
}

static int
capability_routine_compare(const void *a, const void *b)
{
    uint32_t left = (*(CallableCapabilityRoutine *const *)a)->id;
    uint32_t right = (*(CallableCapabilityRoutine *const *)b)->id;
    return (left > right) - (left < right);
}

CallableCapabilityRoutine *
callable_capability_find_routine(struct CallableCapabilityStore *store, uint32_t id)
{
    size_t lo = 0, hi = store->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (store->index[mid]->id == id)
            return store->index[mid];
        if (store->index[mid]->id < id) lo = mid + 1;
        else hi = mid;
    }
    return NULL;
}

static CapabilityInstance *capability_instance(SemanticContext *,
    CallableCapabilityRoutine *, CapabilityTarget *, bool);

/* Only declaration candidates carry authority the caller can name; a formal
 * or binding candidate still belongs to the callee's own frame, and one
 * unnamed return leaves the whole set an under-approximation. */
static CapabilityResultTarget *
capability_routine_result_candidates(const CallableCapabilityRoutine *routine)
{
    if (routine == NULL || routine->result_unknown || routine->results == NULL)
        return NULL;
    for (const CapabilityResultTarget *r = routine->results; r != NULL; r = r->next)
        if (r->target.kind != CAP_DECL)
            return NULL;
    return routine->results;
}

/* A callable value can cross frames before it is invoked, so the call that
 * produced it may sit in another routine. Declaration candidates are global
 * facts, so the caller can still name the authority it inherits. */
static CapabilityResultTarget *
capability_foreign_result_candidates(struct CallableCapabilityStore *store,
                                     uint32_t site_id)
{
    for (CallableCapabilityRoutine *r = store->routines; r != NULL; r = r->next)
        for (CapabilityCall *call = r->calls; call != NULL; call = call->next) {
            if (ast_node_stable_id(call->site) != site_id)
                continue;
            if (call->callee.kind != CAP_DECL)
                return NULL;
            return capability_routine_result_candidates(
                callable_capability_find_routine(store, call->callee.id));
        }
    return NULL;
}

static CapabilityTarget
capability_resolve(SemanticContext *ctx, CapabilityInstance *context,
                   CapabilityTarget value, size_t remaining,
                   CapabilityResultTarget **candidates_out)
{
    struct CallableCapabilityStore *store = ctx->callable_capabilities;
    /* Binding aliases always refer to earlier lexical declarations. A broken
     * cycle or absent provenance remains unresolved, never a pure mask. */
    while (remaining-- > 0) {
        if (value.kind == CAP_RESULT) {
            CapabilityCall *call = context->routine->calls;
            while (call != NULL && ast_node_stable_id(call->site) != value.id)
                call = call->next;
            if (call == NULL) {
                if (candidates_out != NULL)
                    *candidates_out = capability_foreign_result_candidates(
                        store, value.id);
                return (CapabilityTarget){0};
            }
            CapabilityTarget target = capability_resolve(ctx, context,
                call->callee, remaining, NULL);
            CallableCapabilityRoutine *callee = target.kind == CAP_DECL
                ? callable_capability_find_routine(store, target.id) : NULL;
            if (callee == NULL || !callee->has_result || callee->count != call->count)
                return (CapabilityTarget){0};
            CapabilityTarget *actuals = callable_capability_allocate_equation(ctx,
                (call->count + 1) * sizeof(CapabilityTarget));
            if (actuals == NULL) return (CapabilityTarget){0};
            for (size_t p = 0; p < call->count; p++)
                actuals[p] = capability_resolve(ctx, context, call->actuals[p],
                    remaining, NULL);
            context = capability_instance(ctx, callee, actuals,
                context->closed);
            if (context == NULL) return (CapabilityTarget){0};
            if (callee->result.kind == CAP_UNKNOWN) {
                /* Several declarations can reach this result. Keep the result
                 * site instead of collapsing it, so a frame that receives the
                 * value as an argument can still name the candidates. */
                CapabilityResultTarget *reachable =
                    capability_routine_result_candidates(callee);
                if (candidates_out != NULL)
                    *candidates_out = reachable;
                return reachable != NULL ? value : (CapabilityTarget){0};
            }
            value = callee->result;
            continue;
        }
        if (value.kind == CAP_BINDING) {
            CapabilityBinding *b = store->bindings;
            while (b != NULL && b->id != value.id) b = b->next;
            if (b == NULL) return (CapabilityTarget){0};
            value = b->value;
            continue;
        }
        if (value.kind == CAP_FORMAL) {
            size_t i = 0;
            while (i < context->routine->count
                   && context->routine->formals[i] != value.id) i++;
            if (i == context->routine->count) return value;
            CapabilityTarget actual = context->actuals[i];
            if (actual.kind == CAP_FORMAL && actual.id == value.id) return actual;
            value = actual;
            continue;
        }
        return value;
    }
    return (CapabilityTarget){0};
}

/* A call through a callable-valued result runs one of the candidates, so
 * the caller inherits the union of their authority. Linking every
 * candidate as a child keeps the fixed point sound without pretending the
 * value site resolved to a single declaration. */
static bool
capability_link_result_candidates(SemanticContext *ctx,
                                  struct CallableCapabilityStore *store,
                                  CapabilityInstance *caller,
                                  CapabilityCall *call,
                                  CapabilityResultTarget *candidates)
{
    if (candidates == NULL)
        return false;
    for (CapabilityResultTarget *c = candidates; c != NULL; c = c->next) {
        CallableCapabilityRoutine *callee = c->target.kind == CAP_DECL
            ? callable_capability_find_routine(store, c->target.id) : NULL;
        if (callee == NULL || callee->count != call->count)
            return false;
        CapabilityTarget *actuals = callable_capability_allocate_equation(ctx,
            (call->count + 1) * sizeof(CapabilityTarget));
        if (actuals == NULL)
            return false;
        for (size_t p = 0; p < call->count; p++)
            actuals[p] = capability_resolve(ctx, caller, call->actuals[p],
                store->equation_count + 1, NULL);
        CapabilityInstance *child = capability_instance(ctx, callee, actuals,
            caller->closed);
        CapabilityParent *parent =
            callable_capability_allocate_equation(ctx, sizeof(*parent));
        if (child == NULL || parent == NULL)
            return false;
        parent->instance = caller;
        parent->next = child->parents;
        child->parents = parent;
    }
    return true;
}

static CapabilityInstance *
capability_instance(SemanticContext *ctx, CallableCapabilityRoutine *routine,
    CapabilityTarget *actuals, bool closed)
{
    struct CallableCapabilityStore *store = ctx->callable_capabilities;
    for (CapabilityInstance *i = routine->instances; i; i = i->routine_next) {
        if (i->closed != closed) continue;
        size_t p = 0;
        for (; p < routine->count; p++)
            if (i->actuals[p].kind != actuals[p].kind
                || i->actuals[p].id != actuals[p].id) break;
        if (p == routine->count) return i;
    }
    CapabilityInstance *instance = callable_capability_allocate_equation(ctx, sizeof(*instance));
    if (instance == NULL) return NULL;
    instance->actuals = callable_capability_allocate_equation(ctx,
        (routine->count + 1) * sizeof(CapabilityTarget));
    if (instance->actuals == NULL) return NULL;
    if (routine->count > 0)
        memcpy(instance->actuals, actuals, routine->count * sizeof(CapabilityTarget));
    instance->routine = routine;
    instance->closed = closed;
    instance->used = routine->direct_mask;
    instance->used_effects = type_effect_mask_closure(routine->direct_effects);
    instance->routine_next = routine->instances;
    routine->instances = instance;
    if (store->last_instance != NULL) store->last_instance->next = instance;
    else store->instances = instance;
    store->last_instance = instance;
    return instance;
}

static void
capability_check_bound(SemanticContext *ctx, CallableCapabilityRoutine *routine,
                       uint32_t used, bool deferred)
{
    ASTNode *node = routine->decl;
    if (node->type != AST_FUNC_DECL || !ast_func_has_caps_clause(node)) return;
    uint32_t missing = used & ~routine->declared_mask;
    uint32_t excess = routine->declared_mask & ~used;
    char used_buf[160], declared_buf[160], difference[160];
    capability_mask_to_diagnostic_string(used, used_buf, sizeof(used_buf));
    capability_mask_to_diagnostic_string(routine->declared_mask,
        declared_buf, sizeof(declared_buf));
    if (missing != 0) {
        capability_mask_to_diagnostic_string(missing, difference, sizeof(difference));
        semantic_error_with_hints(ctx, PGY_CODE_SEM_EFFECT_CONFLICT,
            PGY_CAUSE_EFFECT_INCOMPATIBLE_COMBO, PGY_FIX_SPLIT_EFFECT_FAMILIES, node,
            "Function '%s' is missing declared capabilities: %s (declared: %s, used by body: %s)",
            ast_declaration_name(node), difference, declared_buf, used_buf);
    } else if (ctx->emit_advisories && !deferred && excess != 0) {
        capability_mask_to_diagnostic_string(excess, difference, sizeof(difference));
        semantic_advisory_with_hints(ctx, PGY_CODE_SEM_CAPABILITY_OVER_DECLARED,
            PGY_CAUSE_CAPABILITY_OVER_DECLARED, PGY_FIX_NARROW_CAPS_TO_USED_SET, node,
            "Function '%s' declares capabilities its body never uses: %s (declared: %s, used: %s)",
            ast_declaration_name(node), difference, declared_buf, used_buf);
    }
}

bool
callable_capability_seal(SemanticContext *ctx)
{
    struct CallableCapabilityStore *store = ctx->callable_capabilities;
    if (store == NULL) return !ctx->has_error;
    if (store->failed || ctx->has_error) return false;
    store->index = callable_capability_allocate_equation(ctx, store->count * sizeof(*store->index));
    if (store->index == NULL) return false;
    size_t n = 0;
    for (CallableCapabilityRoutine *r = store->routines; r; r = r->next)
        store->index[n++] = r;
    qsort(store->index, n, sizeof(*store->index), capability_routine_compare);
    if (!callable_capability_seal_dispatch(ctx)) return false;
    for (CapabilityCall *call = store->subscriptions; call != NULL;) {
        CapabilityCall *next = call->next;
        CallableCapabilityRoutine *event = callable_capability_find_routine(store, call->event_id);
        if (event == NULL || event->decl->type != AST_EVENT_DECL) {
            semantic_error(ctx, call->site, "Event subscription capability identity is missing");
            return false;
        }
        call->count = event->count;
        call->actuals = callable_capability_allocate_equation(ctx,
            (call->count + 1) * sizeof(CapabilityTarget));
        if (call->actuals == NULL) return false;
        for (size_t p = 0; p < call->count; p++)
            call->actuals[p] = (CapabilityTarget){CAP_FORMAL, event->formals[p]};
        call->next = event->calls;
        event->calls = call;
        call = next;
    }
    store->subscriptions = NULL;
    for (size_t i = 0; i < n; i++) {
        CallableCapabilityRoutine *r = store->index[i];
        if (r->id == 0 || (i > 0 && store->index[i - 1]->id == r->id)) {
            semantic_error(ctx, r->decl, "Callable capability declaration identity is missing or duplicated");
            return false;
        }
        CapabilityTarget *formals = callable_capability_allocate_equation(ctx,
            (r->count + 1) * sizeof(CapabilityTarget));
        if (formals == NULL) return false;
        for (size_t p = 0; p < r->count; p++)
            formals[p] = (CapabilityTarget){CAP_FORMAL, r->formals[p]};
        if (capability_instance(ctx, r, formals,
                r->count == 0 && r->decl->type != AST_LAMBDA_EXPR && !r->abstract_dispatch) == NULL)
            return false;
    }
    for (CapabilityInstance *i = store->instances; i; i = i->next) {
        if (i->routine->abstract_dispatch && i->routine->calls == NULL)
            i->deferred = true;
        for (CapabilityCall *call = i->routine->calls; call; call = call->next) {
            CapabilityResultTarget *candidates = NULL;
            CapabilityTarget target = capability_resolve(ctx, i, call->callee,
                store->equation_count + 1, &candidates);
            if (target.kind == CAP_CONSTRUCTOR) continue;
            if (target.kind != CAP_DECL) {
                if (!capability_link_result_candidates(ctx, store, i,
                        call, candidates))
                    i->deferred = true;
                continue;
            }
            CallableCapabilityRoutine *callee = callable_capability_find_routine(store, target.id);
            if (callee == NULL || callee->count != call->count) {
                semantic_error(ctx, call->site, "Callable capability target/signature fact is missing");
                return false;
            }
            CapabilityTarget *actuals = callable_capability_allocate_equation(ctx,
                (call->count + 1) * sizeof(CapabilityTarget));
            if (actuals == NULL) return false;
            for (size_t p = 0; p < call->count; p++)
                actuals[p] = capability_resolve(ctx, i, call->actuals[p],
                    store->equation_count + 1, NULL);
            CapabilityInstance *child = capability_instance(ctx, callee, actuals,
                i->closed);
            CapabilityParent *parent = callable_capability_allocate_equation(ctx, sizeof(*parent));
            if (child == NULL || parent == NULL) return false;
            parent->instance = i;
            parent->next = child->parents;
            child->parents = parent;
        }
    }
    CapabilityInstance *head = NULL, *tail = NULL;
    for (CapabilityInstance *i = store->instances; i; i = i->next) {
        i->queued = true;
        if (tail != NULL) tail->queue_next = i;
        else head = i;
        tail = i;
    }
    while (head != NULL) {
        CapabilityInstance *child = head;
        head = child->queue_next;
        if (head == NULL) tail = NULL;
        child->queue_next = NULL;
        child->queued = false;
        for (CapabilityParent *edge = child->parents; edge; edge = edge->next) {
            CapabilityInstance *parent = edge->instance;
            uint32_t used = parent->used | child->used | child->routine->declared_mask;
            uint32_t effects = type_effect_mask_join(parent->used_effects,
                child->used_effects | child->routine->declared_effects);
            bool deferred = parent->deferred || child->deferred;
            if (used == parent->used && effects == parent->used_effects
                && deferred == parent->deferred) continue;
            parent->used = used;
            parent->used_effects = effects;
            parent->deferred = deferred;
            if (!parent->queued) {
                parent->queued = true;
                if (tail != NULL) tail->queue_next = parent;
                else head = parent;
                tail = parent;
            }
        }
    }
    for (size_t r = 0; r < n; r++) {
        uint32_t used = 0;
        uint32_t effects = 0;
        bool unresolved = false;
        bool deferred = false;
        for (CapabilityInstance *i = store->index[r]->instances; i; i = i->routine_next) {
            used |= i->used;
            effects = type_effect_mask_join(effects, i->used_effects);
            unresolved |= i->closed && i->deferred;
            deferred |= i->deferred;
        }
        if (unresolved)
            semantic_error(ctx, store->index[r]->decl,
                "Closed callable capability use has unresolved value provenance");
        capability_check_bound(ctx, store->index[r], used, deferred);
        type_function_set_capabilities(store->index[r]->type,
            used | store->index[r]->declared_mask);
        type_function_set_effects(store->index[r]->type,
            type_effect_mask_join(effects, store->index[r]->declared_effects));
        if (effects != EFFECT_NONE)
            type_function_set_body_summary(store->index[r]->type,
                type_function_body_summary(store->index[r]->type) | BODY_SUMMARY_EFFECTS);
        if (store->index[r]->decl->type == AST_FUNC_DECL)
            type_check_function_effect_contract(store->index[r]->decl, ctx,
                store->index[r]->declared_effects, store->index[r]->has_effect_contract,
                effects);
        ctx->program_capabilities |= used;
    }
    return !ctx->has_error;
}

void
callable_capability_destroy(SemanticContext *ctx)
{
    /* All equations, instances and indexes belong to the semantic scratch arena. */
    ctx->callable_capabilities = NULL;
    ctx->current_callable_capability = NULL;
}
