#include "transpiler_intent_typed_execution.h"

#include <stdlib.h>

#include "transpiler_context.h"
#include "transpiler_intent_prologue_emit.h"
#include "transpiler_symbols.h"
#include "transpiler_type_require.h"

static const MIRIntentStepTransitionFact *
transpiler_intent_transition_by_id(const MIRRoutine *routine,
                                   uint32_t transition_id,
                                   size_t *index_out)
{
    size_t count = mir_routine_intent_step_transition_count(routine);

    for (size_t i = 0; i < count; i++) {
        const MIRIntentStepTransitionFact *row =
            mir_routine_intent_step_transition_at(routine, i);
        if (row != NULL && row->transition_id == transition_id) {
            if (index_out != NULL)
                *index_out = i;
            return row;
        }
    }
    return NULL;
}

static const MIRIntentStepTransitionFact *
transpiler_intent_transition_child(const MIRRoutine *routine,
                                   uint32_t predecessor_transition_id)
{
    const MIRIntentStepTransitionFact *child = NULL;
    size_t count = mir_routine_intent_step_transition_count(routine);

    for (size_t i = 0; i < count; i++) {
        const MIRIntentStepTransitionFact *row =
            mir_routine_intent_step_transition_at(routine, i);
        if (row == NULL || !row->has_predecessor
            || row->predecessor_transition_id != predecessor_transition_id) {
            continue;
        }
        if (child != NULL)
            return NULL;
        child = row;
    }
    return child;
}

static const MIRIntentTerminalTransitionFact *
transpiler_intent_terminal_for(const MIRRoutine *routine,
                               uint32_t source_transition_id,
                               MIRIntentTerminalRole role)
{
    const MIRIntentTerminalTransitionFact *terminal = NULL;
    size_t count = mir_routine_intent_terminal_transition_count(routine);

    for (size_t i = 0; i < count; i++) {
        const MIRIntentTerminalTransitionFact *row =
            mir_routine_intent_terminal_transition_at(routine, i);
        if (row == NULL || row->source_transition_id != source_transition_id
            || row->role != role) {
            continue;
        }
        if (terminal != NULL)
            return NULL;
        terminal = row;
    }
    return terminal;
}

static bool
transpiler_intent_register_typed_plan_locals(TranspilerCtx *ctx,
                                             const MIRRoutine *routine)
{
    size_t count = mir_routine_intent_step_transition_count(routine);

    for (size_t i = 0; i < count; i++) {
        const MIRIntentStepTransitionFact *row =
            mir_routine_intent_step_transition_at(routine, i);
        char c_type[256];
        char success_type[256];
        char failure_type[256];
        if (row == NULL || !row->sealed || row->outcome_result_name == NULL
            || row->outcome_type_name == NULL || row->success.payload_name == NULL
            || row->success.payload_type_name == NULL
            || row->failure.payload_name == NULL
            || row->failure.payload_type_name == NULL
            || !transpiler_require_type_name_c_type_copy(ctx,
                row->outcome_type_name, "typed intent outcome",
                c_type, sizeof(c_type))
            || !transpiler_require_type_name_c_type_copy(ctx,
                row->success.payload_type_name, "typed intent success payload",
                success_type, sizeof(success_type))
            || !transpiler_require_type_name_c_type_copy(ctx,
                row->failure.payload_type_name, "typed intent failure payload",
                failure_type, sizeof(failure_type))) {
            return false;
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s;\n", c_type,
            row->outcome_result_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s;\n", success_type,
            row->success.payload_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s;\n", failure_type,
            row->failure.payload_name);
        register_typed_var(ctx, row->outcome_result_name,
            row->outcome_type_name);
        register_typed_var(ctx, row->success.payload_name,
            row->success.payload_type_name);
        register_typed_var(ctx, row->failure.payload_name,
            row->failure.payload_type_name);
        if (ctx->backend_error != NULL)
            return false;
    }
    return true;
}

static bool
transpiler_intent_emit_predecessor_compensation(
    TranspilerCtx *ctx,
    const MIRRoutine *routine,
    const MIRIntentStepTransitionFact *failed)
{
    const MIRIntentStepTransitionFact *cursor = failed;
    size_t step_count = mir_routine_intent_step_transition_count(routine);
    size_t depth = 0;

    while (cursor != NULL && cursor->has_predecessor) {
        size_t predecessor_index = 0;
        const MIRIntentStepTransitionFact *predecessor =
            transpiler_intent_transition_by_id(routine,
                cursor->predecessor_transition_id, &predecessor_index);
        if (predecessor == NULL
            || (predecessor->compensation_count > 0
                && predecessor->compensations == NULL)) {
            transpiler_set_mir_inventory_missing(ctx,
                "admitted typed intent has invalid predecessor compensation evidence");
            return false;
        }
        if (predecessor->compensation_count > 0) {
            write_indent(ctx);
            codebuf_write(ctx->out,
                "if (__intent_step_completed[%zu]) {\n",
                predecessor_index);
            ctx->indent++;
            for (size_t i = predecessor->compensation_count; i > 0; i--) {
                const MIRIntentCompensationFact *compensation =
                    &predecessor->compensations[i - 1];
                char *expression;

                if (compensation->expression == NULL) {
                    transpiler_set_mir_inventory_missing(ctx,
                        "admitted typed intent has missing predecessor compensation expression");
                    return false;
                }
                expression = emit_expression(compensation->expression, ctx);
                if (expression == NULL || ctx->backend_error != NULL) {
                    free(expression);
                    return false;
                }
                write_indent(ctx);
                codebuf_write(ctx->out, "%s;\n", expression);
                free(expression);
            }
            ctx->indent--;
            write_indent(ctx);
            codebuf_write(ctx->out, "}\n");
        }
        cursor = predecessor;
        depth++;
        if (depth > step_count) {
            transpiler_set_mir_inventory_missing(ctx,
                "admitted typed intent predecessor chain is cyclic");
            return false;
        }
    }
    return true;
}

bool
transpiler_emit_typed_intent_execution(
    ASTNode *node,
    TranspilerCtx *ctx,
    const MIRRoutine *routine,
    const IntentBindingMetadataView *bindings)
{
    const MIRIntentStepTransitionFact *root = NULL;
    size_t step_count = mir_routine_intent_step_transition_count(routine);
    size_t terminal_count =
        mir_routine_intent_terminal_transition_count(routine);
    bool has_compensation = false;

    if (step_count == 0 || terminal_count == 0)
        return false;
    for (size_t i = 0; i < step_count; i++) {
        const MIRIntentStepTransitionFact *row =
            mir_routine_intent_step_transition_at(routine, i);
        if (row == NULL || !row->sealed || row->transition_id == 0
            || row->outcome_expression == NULL) {
            transpiler_set_mir_inventory_missing(ctx,
                "admitted typed intent has an incomplete step transition");
            return false;
        }
        if (!row->has_predecessor) {
            if (root != NULL) {
                transpiler_set_mir_inventory_missing(ctx,
                    "admitted typed intent has multiple root transitions");
                return false;
            }
            root = row;
        }
        if (row->compensation_count > 0)
            has_compensation = true;
    }
    if (root == NULL) {
        transpiler_set_mir_inventory_missing(ctx,
            "admitted typed intent has no root transition");
        return false;
    }
    for (size_t i = 0; i < terminal_count; i++) {
        const MIRIntentTerminalTransitionFact *terminal =
            mir_routine_intent_terminal_transition_at(routine, i);
        if (terminal == NULL || !terminal->sealed
            || terminal->expression == NULL
            || transpiler_intent_transition_by_id(
                routine, terminal->source_transition_id, NULL) == NULL) {
            transpiler_set_mir_inventory_missing(ctx,
                "admitted typed intent has an incomplete terminal transition");
            return false;
        }
    }

    if (!transpiler_emit_intent_signature_and_entry(
            node, ctx, has_compensation, step_count, bindings,
            false, routine)
        || !transpiler_intent_register_typed_plan_locals(ctx, routine)) {
        return false;
    }
    write_indent(ctx);
    codebuf_write(ctx->out, "goto __intent_transition_%u;\n",
        root->transition_id);

    for (size_t i = 0; i < step_count; i++) {
        const MIRIntentStepTransitionFact *row =
            mir_routine_intent_step_transition_at(routine, i);
        const MIRIntentStepTransitionFact *child =
            transpiler_intent_transition_child(routine, row->transition_id);
        const MIRIntentTerminalTransitionFact *success_terminal =
            transpiler_intent_terminal_for(routine, row->transition_id,
                MIR_INTENT_TERMINAL_SUCCESS);
        const MIRIntentTerminalTransitionFact *failure_terminal =
            transpiler_intent_terminal_for(routine, row->transition_id,
                MIR_INTENT_TERMINAL_FAILURE);
        char *outcome;

        if ((child == NULL) == (success_terminal == NULL)
            || failure_terminal == NULL) {
            transpiler_set_mir_inventory_missing(ctx,
                "admitted typed intent successor/terminal topology is invalid");
            return false;
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "__intent_transition_%u:\n",
            row->transition_id);
        if (ctx->uses_intent_observability) {
            write_indent(ctx);
            codebuf_write(ctx->out,
                "pgy_intent_trace_step_export(__intent_handle, \"%s\", \"%s\");\n",
                row->step_name, row->where_zone_name);
        }
        outcome = emit_expression(row->outcome_expression, ctx);
        if (outcome == NULL || ctx->backend_error != NULL) {
            free(outcome);
            return false;
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "%s = %s;\n",
            row->outcome_result_name, outcome);
        free(outcome);
        write_indent(ctx);
        codebuf_write(ctx->out, "if (%s.tag == (%s_Tag)%zu) {\n",
            row->outcome_result_name, row->outcome_enum_name,
            row->success.variant_index);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "%s = %s.%s._0;\n",
            row->success.payload_name, row->outcome_result_name,
            row->success.variant_name);
        if (has_compensation) {
            write_indent(ctx);
            codebuf_write(ctx->out,
                "__intent_step_completed[%zu] = true;\n", i);
        }
        if (ctx->uses_intent_observability) {
            write_indent(ctx);
            codebuf_write(ctx->out,
                "pgy_intent_trace_step_ok_export(__intent_handle, \"%s\");\n",
                row->step_name);
        }
        write_indent(ctx);
        if (child != NULL) {
            codebuf_write(ctx->out, "goto __intent_transition_%u;\n",
                child->transition_id);
        } else {
            codebuf_write(ctx->out, "goto __intent_terminal_%u;\n",
                success_terminal->terminal_transition_id);
        }
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "if (%s.tag == (%s_Tag)%zu) {\n",
            row->outcome_result_name, row->outcome_enum_name,
            row->failure.variant_index);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "%s = %s.%s._0;\n",
            row->failure.payload_name, row->outcome_result_name,
            row->failure.variant_name);
        if (ctx->uses_intent_observability) {
            write_indent(ctx);
            codebuf_write(ctx->out,
                "pgy_intent_trace_fail_export(__intent_handle, \"outcome:%s\");\n",
                row->step_name);
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "goto __intent_terminal_%u;\n",
            failure_terminal->terminal_transition_id);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
        write_indent(ctx);
        codebuf_write(ctx->out,
            "pgy_runtime_panic_internal_invariant_export(\"typed intent outcome tag escaped admitted branches\");\n");
    }

    for (size_t i = 0; i < terminal_count; i++) {
        const MIRIntentTerminalTransitionFact *terminal =
            mir_routine_intent_terminal_transition_at(routine, i);
        const MIRIntentStepTransitionFact *source =
            transpiler_intent_transition_by_id(
                routine, terminal->source_transition_id, NULL);
        char *result;

        write_indent(ctx);
        codebuf_write(ctx->out, "__intent_terminal_%u:\n",
            terminal->terminal_transition_id);
        if (terminal->role == MIR_INTENT_TERMINAL_FAILURE
            && !transpiler_intent_emit_predecessor_compensation(
                ctx, routine, source)) {
            return false;
        }
        result = emit_expression(terminal->expression, ctx);
        if (result == NULL || ctx->backend_error != NULL) {
            free(result);
            return false;
        }
        write_indent(ctx);
        codebuf_write(ctx->out,
            "if (__intent_handle != 0) pgy_intent_exit_export(__intent_handle);\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "return %s;\n", result);
        free(result);
    }
    ctx->indent--;
    codebuf_write(ctx->out, "}\n");
    return true;
}
