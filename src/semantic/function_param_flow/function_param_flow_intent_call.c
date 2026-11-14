/* Intent call flow owner.
 *
 * An intent is not a routine the backing frames enter: its steps call
 * actions on its bindings. The effect of an intent call on its arguments'
 * storage is the effect of those action calls on the bindings the arguments
 * bind, read from the actions' parameter summaries; an argument given to an
 * action on a subject is also kept by that subject.
 *
 * This owner enumerates one intent shape: exclusive, no typed result, Bool
 * literal success and failure, no rollback, and steps that each name their
 * zone, make one action call whose receiver and arguments are intent
 * bindings or literals, and expect its outcome. Any other clause leaves the
 * frame incomplete; a clause added to intents must be added here before an
 * intent using it is admitted. */

#include "function_param_flow_summary_internal.h"
#include "function_param_publication_owner.h"
#include "../type_checker/type_checker_internal.h"
#include <stdint.h>
#include <string.h>

static bool bool_literal_or_absent(const ASTNode *node)
{
    return node == NULL || node->type == AST_BOOLEAN;
}

static bool identifier_or_absent(const ASTNode *node)
{
    return node == NULL || node->type == AST_IDENTIFIER;
}

static bool intent_shape_enumerated(const ASTNode *intent)
{
    if (ast_intent_decl_is_concurrent(intent) || ast_intent_decl_has_typed_result(intent) ||
        ast_intent_decl_return_type(intent) != NULL || ast_intent_decl_priority_expr(intent) != NULL ||
        ast_intent_decl_rollback_declared(intent) || ast_intent_decl_success_terminal_expr(intent) != NULL ||
        ast_intent_decl_failure_terminal_count(intent) != 0 ||
        !bool_literal_or_absent(ast_intent_decl_success_expr(intent)) ||
        !bool_literal_or_absent(ast_intent_decl_failure_expr(intent))) return false;
    size_t count = 0;
    ASTNode **steps = ast_intent_decl_steps(intent, &count);
    for (size_t s = 0; s < count; s++) {
        const ASTNode *step = steps[s];
        if (step == NULL || step->type != AST_INTENT_STEP || ast_intent_step_intent_expr(step) != NULL ||
            ast_intent_step_on_expr_count(step) != 1 || ast_intent_step_compensate_expr_count(step) != 0 ||
            ast_intent_step_pre_expr(step) != NULL || ast_intent_step_guard_expr(step) != NULL ||
            ast_intent_step_post_expr(step) != NULL || ast_intent_step_invariant_expr(step) != NULL ||
            ast_intent_step_transfer_from_alias(step) != NULL || ast_intent_step_transfer_to_alias(step) != NULL ||
            !identifier_or_absent(ast_intent_step_using_expr(step)) ||
            !identifier_or_absent(ast_intent_step_expect_expr(step))) return false;
    }
    return true;
}

/* The ordinal of the intent binding an identifier names, which is the
 * ordinal of the call argument bound to it. */
static size_t intent_binding_ordinal(const ASTNode *intent, const ASTNode *identifier)
{
    if (identifier == NULL || identifier->type != AST_IDENTIFIER) return SIZE_MAX;
    uint32_t id = ast_identifier_binding_syntax_id(identifier);
    size_t count = 0;
    ASTNode **bindings = ast_intent_decl_bindings(intent, &count);
    for (size_t k = 0; id != 0 && k < count; k++)
        if (bindings[k] != NULL && ast_node_stable_id(bindings[k]) == id) return k;
    return SIZE_MAX;
}

/* One step's action call. Each call argument it passes on escapes, stays or
 * moves into another argument as the action's summary for that formal says. */
static void intent_action_effect(ReturnedBacking *state, const ASTNode *intent, ASTNode *call, ASTNode *action)
{
    SemanticContext *ctx = state->store->ctx;
    FunctionParamFlowBackingFact *fact = state->fact;
    ASTNode *receiver = NULL;
    ASTNode *method = action != NULL && action->type == AST_CALL
        ? semantic_param_publication_direct_callee(ctx, action) : NULL;
    if (method == NULL || !semantic_param_publication_instance_receiver(ctx, action, &receiver)) {
        fact->complete = false; return;
    }
    /* A subject is its identity: an action called on it may keep what it is
     * given in the subject's fields, where the caller reads it afterwards. */
    ASTNode *keeper = NULL;
    if (receiver != NULL) {
        size_t k = intent_binding_ordinal(intent, receiver);
        const uint64_t *bits = k == SIZE_MAX ? NULL : backing_origin(state, ast_call_argument(call, k));
        SemanticPublishedValue value = semantic_param_publication_value(ctx, receiver);
        if (bits == NULL || !value.ok || value.meta) { fact->complete = false; return; }
        if (type_is_subject_type(value.type, ctx)) keeper = ast_call_argument(call, k);
        /* A receiver carrying this frame's storage into an action that may
         * change it follows the action's summary for its self formal. */
        if (backing_any_origin(state, bits) &&
            function_param_flow_receiver_readonly_current(state->store, action, true) == NULL) {
            size_t entry = function_param_flow_receiver_formal_summary(state, method);
            FuncParam *self = ast_func_param(method, 0);
            if (entry == SIZE_MAX) (void)function_param_flow_receiver_unsupported(state, action, method);
            if (entry == SIZE_MAX || self == NULL) { fact->complete = false; return; }
            unsigned outward_effects = SLOT_PARAM_SUMMARY_NONE;
            if (!function_param_flow_entry_outward_effects_current(
                    state->store, entry, &outward_effects)) {
                fact->complete = false;
                return;
            }
            backing_effect(state, bits, outward_effects &
                (SLOT_PARAM_SUMMARY_CALL_ESCAPE |
                 SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE));
            if ((outward_effects & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0 ||
                self->mode == PARAM_MODE_OWN)
                backing_effect(state, bits, SLOT_PARAM_SUMMARY_CALL_ESCAPE);
            if (self->mode == PARAM_MODE_MUT_REF &&
                !backing_inout_argument_leaves_input(state))
                backing_movement_effect(state, bits,
                    SLOT_PARAM_SUMMARY_CALL_ESCAPE);
            for (size_t p = 0; p < ast_call_arg_count(action) && fact->complete; p++) {
                size_t formal = 0, target = intent_binding_ordinal(intent, ast_call_argument(action, p));
                ASTNode *arg = ast_call_argument(action, p);
                if (arg != NULL && (arg->type == AST_STRING || arg->type == AST_NUMBER || arg->type == AST_BOOLEAN)) continue;
                if (target == SIZE_MAX || !semantic_param_publication_formal_for_actual(ctx, action, p, &formal) ||
                    formal >= state->store->entries[entry].output_count) { fact->complete = false; return; }
                FuncParam *output = ast_func_param(method, formal);
                if (output != NULL && (output->mode == PARAM_MODE_REF || output->mode == PARAM_MODE_MUT_REF) &&
                    state->store->entries[entry].output_dependencies[formal])
                    backing_store(state, ast_call_argument(call, target), bits, false, NULL);
            }
        }
    }
    size_t count = ast_call_arg_count(action);
    for (size_t i = 0; i < count && fact->complete; i++) {
        ASTNode *arg = ast_call_argument(action, i);
        if (arg != NULL && (arg->type == AST_STRING || arg->type == AST_NUMBER || arg->type == AST_BOOLEAN)) continue;
        size_t k = intent_binding_ordinal(intent, arg), formal = 0;
        const uint64_t *bits = k == SIZE_MAX ? NULL : backing_origin(state, ast_call_argument(call, k));
        if (bits == NULL || !semantic_param_publication_formal_for_actual(ctx, action, i, &formal)) {
            fact->complete = false; return;
        }
        if (!backing_any_origin(state, bits)) continue;
        FuncParam *param = ast_func_param(method, formal);
        unsigned outward_effects = SLOT_PARAM_SUMMARY_NONE;
        bool outward_ready =
            function_param_flow_callee_outward_effects_current(
                &state->lookup, action, i, &outward_effects);
        bool relation_ready = function_param_flow_callee_relation_ready(
            &state->lookup, action, i);
        if (outward_ready)
            backing_effect(state, bits, outward_effects &
                (SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE));
        /* The intent's value is only its Bool verdict, so an action result
         * carrying the argument stays inside the intent: an escape. */
        if ((outward_ready &&
             (outward_effects & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0) ||
            param == NULL || param->mode == PARAM_MODE_OWN)
            backing_effect(state, bits, SLOT_PARAM_SUMMARY_CALL_ESCAPE);
        if (param != NULL && param->mode == PARAM_MODE_MUT_REF &&
            !backing_inout_argument_leaves_input(state))
            backing_movement_effect(state, bits,
                SLOT_PARAM_SUMMARY_CALL_ESCAPE);
        if (!relation_ready)
            function_param_flow_callee_unfinished(&state->lookup, action, i, fact);
        else if (!outward_ready) {
            fact->complete = false;
            return;
        }
        if (keeper != NULL) backing_store(state, keeper, bits, false, NULL);
        for (size_t p = 0; p < count && fact->complete; p++) {
            size_t output_formal = 0;
            if (!semantic_param_publication_formal_for_actual(ctx, action, p, &output_formal)) {
                fact->complete = false; break;
            }
            FuncParam *output = ast_func_param(method, output_formal);
            if (output == NULL) { fact->complete = false; break; }
            if (output->mode != PARAM_MODE_MUT_REF && output->mode != PARAM_MODE_REF) continue;
            if (output->mode == PARAM_MODE_REF && output->type != NULL && ast_type_name(output->type) != NULL &&
                strcmp(ast_type_name(output->type), "String") == 0) continue;
            size_t target = intent_binding_ordinal(intent, ast_call_argument(action, p));
            bool retained = false;
            if (target == SIZE_MAX) { fact->complete = false; break; }
            if (!function_param_flow_output_dependency_for_call(&state->lookup, action, i, p, &retained))
                function_param_flow_callee_unfinished(&state->lookup, action, i, fact);
            else if (retained) backing_store(state, ast_call_argument(call, target), bits, false, NULL);
        }
    }
}

bool function_param_flow_intent_step_call(SemanticContext *ctx, const ASTNode *call, const ASTNode *action)
{
    if (call == NULL || call->type != AST_CALL || action == NULL || action == call) return false;
    ASTNode *intent = semantic_param_publication_intent_callee(ctx, call);
    size_t count = 0;
    ASTNode **steps = intent != NULL ? ast_intent_decl_steps(intent, &count) : NULL;
    for (size_t s = 0; action != NULL && s < count; s++) {
        size_t on = 0;
        ASTNode **actions = steps[s] != NULL ? ast_intent_step_on_exprs(steps[s], &on) : NULL;
        for (size_t a = 0; a < on; a++) if (actions[a] == action) return true;
    }
    return false;
}

ASTNode *function_param_flow_intent_action_callee(SemanticContext *ctx, ASTNode *call, size_t step)
{
    ASTNode *intent = semantic_param_publication_intent_callee(ctx, call);
    size_t count = 0, on = 0;
    ASTNode **steps = intent != NULL ? ast_intent_decl_steps(intent, &count) : NULL;
    if (step >= count || steps[step] == NULL) return NULL;
    ASTNode **action = ast_intent_step_on_exprs(steps[step], &on);
    return on == 1 && action[0] != NULL && action[0]->type == AST_CALL
        ? semantic_param_publication_direct_callee(ctx, action[0]) : NULL;
}

bool function_param_flow_intent_call(ReturnedBacking *state, ASTNode *call)
{
    ASTNode *intent = semantic_param_publication_intent_callee(state->store->ctx, call);
    if (intent == NULL) return false;
    size_t count = ast_call_arg_count(call);
    for (size_t i = 0; i < count; i++) {
        if (!semantic_param_publication_checked_argument(state->store->ctx, call, i)) {
            state->fact->complete = false; return true;
        }
        backing_consumers(state, ast_call_argument(call, i));
    }
    if (!intent_shape_enumerated(intent) || ast_intent_decl_binding_count(intent) != count) {
        state->fact->complete = false; return true;
    }
    size_t steps = 0;
    ASTNode **step = ast_intent_decl_steps(intent, &steps);
    for (size_t s = 0; s < steps && state->fact->complete; s++) {
        size_t on = 0;
        ASTNode **action = ast_intent_step_on_exprs(step[s], &on);
        intent_action_effect(state, intent, call, on == 1 ? action[0] : NULL);
    }
    return true;
}
