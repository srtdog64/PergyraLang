/* Solving half of the callable capability equation.
 * Resolves value provenance, interns instances and seals the fixed point;
 * the recording half owns how those rows were observed. */
#include "callable_capability_equations_internal.h"
#include "capability_analyze.h"
#include "type_checker_internal.h"
#include "diag_codes.h"
#include "function_param_publication_owner.h"
#include "function_param_flow_summary_internal.h"

#include <stdlib.h>
#include <string.h>

const ASTNode *callable_capability_current_declaration(const SemanticContext *ctx)
{
    return ctx != NULL && ctx->current_callable_capability != NULL
        ? ctx->current_callable_capability->decl : NULL;
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
capability_capture_environment(SemanticContext *ctx, CapabilityTarget target,
                               bool *requires_local)
{
    if (target.kind == CAP_CONSTRUCTOR) return true;
    if (target.kind != CAP_DECL) return false;
    CallableCapabilityRoutine *routine = callable_capability_find_routine(ctx->callable_capabilities, target.id);
    if (routine == NULL || ast_node_stable_id(routine->decl) != target.id) return false;
    if (routine->decl->type != AST_LAMBDA_EXPR) return true;
    SemanticPublishedDeferred view;
    if (!semantic_param_publication_query_deferred(ctx, routine->decl, &view)) return false;
    for (size_t c = 0; c < view.capture_count; c++)
        *requires_local |= !view.captures[c].copy_escape;
    return true;
}

static bool
capability_value_environment(SemanticContext *ctx, CapabilityInstance *context,
                             CapabilityTarget value, bool *requires_local)
{
    *requires_local = false;
    CapabilityResultTarget *candidates = NULL;
    CapabilityTarget target = capability_resolve(ctx, context, value,
        ctx->callable_capabilities->equation_count + 1, &candidates);
    /* An open formal is symbolic, not a copy-only receipt. Its concrete
     * instances and actual argument boundaries are checked independently. */
    if (target.kind == CAP_FORMAL && !context->closed) return true;
    if (target.kind == CAP_DECL || target.kind == CAP_CONSTRUCTOR)
        return capability_capture_environment(ctx, target, requires_local);
    if (candidates == NULL) return false;
    for (CapabilityResultTarget *c = candidates; c != NULL; c = c->next)
        if (!capability_capture_environment(ctx, c->target, requires_local)) return false;
    return true;
}

static bool
capability_environment_refusal(SemanticContext *ctx, ASTNode *site)
{
    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE, site,
        "Callable environment escape lacks a current independent capture lifetime proof");
    return false;
}

static bool
capability_actual_environments_admitted(SemanticContext *ctx,
    CapabilityInstance *caller, CapabilityCall *call, CallableCapabilityRoutine *callee)
{
    for (size_t p = 0; p < call->count; p++) {
        bool requires_local = false;
        bool known = capability_value_environment(ctx, caller, call->actuals[p], &requires_local);
        if (!known) return capability_environment_refusal(ctx, call->site);
        if (known && !requires_local) continue;
        if (callee == NULL || callee->decl->type != AST_FUNC_DECL ||
            ast_func_body(callee->decl) == NULL || p >= callee->count ||
            callee->formal_ordinals == NULL) return capability_environment_refusal(ctx, call->site);
        size_t ordinal = callee->formal_ordinals[p];
        /* Synthetic dispatch rows already name the exact implementation's
         * formal. Source calls must also retain their original argument slot. */
        if ((call->site->type == AST_CALL || call->site->type == AST_EVENT_INVOKE) &&
            (call->actual_ordinals == NULL || call->actual_ordinals[p] != ordinal))
            return capability_environment_refusal(ctx, call->site);
        if (call->site->type == AST_CALL &&
            !semantic_param_publication_checked_argument(ctx, call->site, ordinal))
            return capability_environment_refusal(ctx, call->site);
        unsigned effect = function_param_flow_summary_for_param(ctx, callee->decl, ordinal);
        if (ctx->has_error) return false;
        if ((effect & (SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE |
                       SLOT_PARAM_SUMMARY_RETURN_ESCAPE)) != 0)
            return capability_environment_refusal(ctx, call->site);
    }
    return true;
}

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
        if (!capability_actual_environments_admitted(ctx, caller, call, callee)) return false;
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
        for (CapabilityRetainedValue *r = i->routine->retained_values; r != NULL; r = r->next) {
            bool requires_local = false;
            if (r->site_id != ast_node_stable_id(r->site) || r->value_id != ast_node_stable_id(r->value) ||
                !semantic_param_publication_value(ctx, r->value).ok ||
                !capability_value_environment(ctx, i, r->target, &requires_local) || requires_local)
                return capability_environment_refusal(ctx, r->site);
        }
        if (i->routine->result_unknown)
            return capability_environment_refusal(ctx, i->routine->unknown_result_site);
        for (CapabilityResultTarget *r = i->routine->results; r != NULL; r = r->next) {
            bool requires_local = false;
            if (!capability_value_environment(ctx, i, r->target, &requires_local) || requires_local)
                return capability_environment_refusal(ctx, r->site);
        }
        if (i->routine->abstract_dispatch && i->routine->calls == NULL)
            i->deferred = true;
        for (CapabilityCall *call = i->routine->calls; call; call = call->next) {
            CapabilityResultTarget *candidates = NULL;
            CapabilityTarget target = capability_resolve(ctx, i, call->callee,
                store->equation_count + 1, &candidates);
            if (call->event_id != 0) {
                bool requires_local = false;
                if (!capability_value_environment(ctx, i, call->callee, &requires_local) || requires_local)
                    return capability_environment_refusal(ctx, call->site);
            }
            if (target.kind == CAP_CONSTRUCTOR) {
                if (!capability_actual_environments_admitted(ctx, i, call, NULL)) return false;
                continue;
            }
            if (target.kind != CAP_DECL) {
                if (!capability_link_result_candidates(ctx, store, i,
                        call, candidates)) {
                    if (ctx->has_error) return false;
                    if (!capability_actual_environments_admitted(ctx, i, call, NULL)) return false;
                    i->deferred = true;
                }
                continue;
            }
            CallableCapabilityRoutine *callee = callable_capability_find_routine(store, target.id);
            if (callee == NULL || callee->count != call->count) {
                semantic_error(ctx, call->site, "Callable capability target/signature fact is missing");
                return false;
            }
            if (!capability_actual_environments_admitted(ctx, i, call, callee)) return false;
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
