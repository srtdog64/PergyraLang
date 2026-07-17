#include "rir.h"
#include "rir_internal.h"

#include <stdlib.h>
#include <string.h>

static void
rir_dump_flow_semantics(FILE *out, unsigned int flags)
{
    bool wrote = false;

    if (flags == RIR_FLOW_NONE) {
        fputs("-", out);
        return;
    }
    if ((flags & RIR_FLOW_AUTHORITY) != 0U) {
        fputs("authority", out);
        wrote = true;
    }
    if ((flags & RIR_FLOW_PROJECTION) != 0U) {
        if (wrote)
            fputc('|', out);
        fputs("projection", out);
        wrote = true;
    }
    if ((flags & RIR_FLOW_WORLD_HANDOFF) != 0U) {
        if (wrote)
            fputc('|', out);
        fputs("world-handoff", out);
        wrote = true;
    }
    if ((flags & RIR_FLOW_INVALIDATION) != 0U) {
        if (wrote)
            fputc('|', out);
        fputs("invalidation", out);
        wrote = true;
    }
    if ((flags & RIR_FLOW_AUTHORITY_LOSS) != 0U) {
        if (wrote)
            fputc('|', out);
        fputs("authority-loss", out);
        wrote = true;
    }
    if ((flags & RIR_FLOW_PROJECTION_INVALIDATION) != 0U) {
        if (wrote)
            fputc('|', out);
        fputs("projection-invalidation", out);
    }
}

void
rir_destroy(RIRProgram *rir)
{
    if (rir == NULL)
        return;
    for (size_t i = 0; i < rir->scope_count; i++) {
        free(rir->scopes[i].facts);
        free(rir->scopes[i].ops);
        free(rir->scopes[i].state_summaries);
        if (rir->scopes[i].resource_flow_symbols != NULL) {
            for (size_t j = 0;
                 j < rir->scopes[i].resource_flow_symbol_count; j++)
                free(rir->scopes[i].resource_flow_symbols[j].name);
        }
        free(rir->scopes[i].resource_flow_symbols);
        free(rir->scopes[i].function_param_flow_summaries);
        rir_free_flow_blocks(&rir->scopes[i]);
    }
    free(rir->scopes);
    free(rir);
}

size_t
rir_scope_function_param_flow_summary_count(const RIRScope *scope)
{
    return scope != NULL ? scope->function_param_flow_summary_count : 0;
}

const RIRFunctionParamFlowSummary *
rir_scope_function_param_flow_summary_at(const RIRScope *scope, size_t index)
{
    if (scope == NULL || index >= scope->function_param_flow_summary_count)
        return NULL;
    return &scope->function_param_flow_summaries[index];
}

size_t
rir_scope_resource_flow_symbol_count(const RIRScope *scope)
{
    return scope != NULL ? scope->resource_flow_symbol_count : 0;
}

const RIRResourceFlowSymbol *
rir_scope_resource_flow_symbol_at(const RIRScope *scope, size_t index)
{
    if (scope == NULL || index >= scope->resource_flow_symbol_count)
        return NULL;
    return &scope->resource_flow_symbols[index];
}

void
rir_scope_inventory_from_program(const RIRProgram *rir,
                                 RIRScopeInventory *inventory)
{
    if (inventory == NULL)
        return;
    inventory->scopes = NULL;
    inventory->count = 0;
    if (rir != NULL && rir->scopes != NULL) {
        inventory->scopes = rir->scopes;
        inventory->count = rir->scope_count;
    }
}

const RIRScope *
rir_scope_inventory_get(const RIRScopeInventory *inventory, size_t index)
{
    if (inventory == NULL || inventory->scopes == NULL
        || index >= inventory->count)
        return NULL;
    return &inventory->scopes[index];
}

void
rir_mutable_scope_inventory_from_program(
        RIRProgram *rir,
        RIRMutableScopeInventory *inventory)
{
    if (inventory == NULL)
        return;
    inventory->scopes = NULL;
    inventory->count = 0;
    if (rir != NULL && rir->scopes != NULL) {
        inventory->scopes = rir->scopes;
        inventory->count = rir->scope_count;
    }
}

RIRScope *
rir_mutable_scope_inventory_get(
        const RIRMutableScopeInventory *inventory,
        size_t index)
{
    if (inventory == NULL || inventory->scopes == NULL
        || index >= inventory->count)
        return NULL;
    return &inventory->scopes[index];
}

RIRScopeKind
rir_scope_kind(const RIRScope *scope)
{
    return scope != NULL ? scope->kind : RIR_SCOPE_FUNCTION;
}

const char *
rir_scope_name(const RIRScope *scope)
{
    return scope != NULL ? scope->name : NULL;
}

const char *
rir_scope_owner_name(const RIRScope *scope)
{
    return scope != NULL ? scope->owner_name : NULL;
}

const char *
rir_scope_display_name(const RIRScope *scope)
{
    const char *name = rir_scope_name(scope);
    return name != NULL ? name : "(anonymous)";
}

bool
rir_scope_has_state_errors(const RIRScope *scope)
{
    return scope != NULL && scope->has_state_errors;
}

size_t
rir_scope_fact_count(const RIRScope *scope)
{
    return scope != NULL ? scope->fact_count : 0;
}

const RIRFact *
rir_scope_fact_at(const RIRScope *scope, size_t index)
{
    if (scope == NULL || scope->facts == NULL || index >= scope->fact_count)
        return NULL;
    return &scope->facts[index];
}

size_t
rir_scope_op_count(const RIRScope *scope)
{
    return scope != NULL ? scope->op_count : 0;
}

const RIROp *
rir_scope_op_at(const RIRScope *scope, size_t index)
{
    if (scope == NULL || scope->ops == NULL || index >= scope->op_count)
        return NULL;
    return &scope->ops[index];
}

const RIROp *
rir_scope_find_op_by_ast(const RIRScope *scope, const ASTNode *ast)
{
    if (scope == NULL || ast == NULL)
        return NULL;
    for (size_t i = 0; i < scope->op_count; i++) {
        if (scope->ops[i].ast == ast)
            return &scope->ops[i];
    }
    return NULL;
}

size_t
rir_scope_state_summary_count(const RIRScope *scope)
{
    return scope != NULL ? scope->state_summary_count : 0;
}

const RIRStateSummary *
rir_scope_state_summary_at(const RIRScope *scope, size_t index)
{
    if (scope == NULL || scope->state_summaries == NULL
        || index >= scope->state_summary_count) {
        return NULL;
    }
    return &scope->state_summaries[index];
}

const RIRStateSummary *
rir_scope_find_state_summary(const RIRScope *scope, const char *name)
{
    if (scope == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < rir_scope_state_summary_count(scope); i++) {
        const RIRStateSummary *summary =
            rir_scope_state_summary_at(scope, i);
        if (summary != NULL
            && summary->name != NULL
            && strcmp(summary->name, name) == 0) {
            return summary;
        }
    }
    return NULL;
}

const RIRFact *
rir_scope_find_fact_by_name_kind(const RIRScope *scope,
                                 RIRFactKind kind,
                                 const char *name)
{
    if (scope == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < rir_scope_fact_count(scope); i++) {
        const RIRFact *fact = rir_scope_fact_at(scope, i);
        if (fact != NULL
            && fact->kind == kind
            && fact->name != NULL
            && strcmp(fact->name, name) == 0) {
            return fact;
        }
    }
    return NULL;
}

const RIRFact *
rir_scope_find_projection_fact(const RIRScope *scope, const char *name)
{
    return rir_scope_find_fact_by_name_kind(scope,
                                            RIR_FACT_PROJECTION,
                                            name);
}

bool
rir_scope_has_capability_fact(const RIRScope *scope,
                              const char *participant,
                              const char *ability)
{
    if (scope == NULL || participant == NULL || ability == NULL)
        return false;
    for (size_t i = 0; i < rir_scope_fact_count(scope); i++) {
        const RIRFact *fact = rir_scope_fact_at(scope, i);
        if (fact != NULL
            && fact->kind == RIR_FACT_CAPABILITY
            && fact->name != NULL
            && strcmp(fact->name, participant) == 0
            && fact->arg0 != NULL
            && strcmp(fact->arg0, ability) == 0) {
            return true;
        }
    }
    return false;
}

unsigned int
rir_scope_conservative_semantics(const RIRScope *scope)
{
    return scope != NULL ? scope->conservative_semantics : RIR_FLOW_NONE;
}

size_t
rir_scope_flow_block_count(const RIRScope *scope)
{
    return scope != NULL ? scope->flow_block_count : 0;
}

const RIRFlowBlock *
rir_scope_flow_block_at(const RIRScope *scope, size_t index)
{
    if (scope == NULL || scope->flow_blocks == NULL
        || index >= scope->flow_block_count) {
        return NULL;
    }
    return &scope->flow_blocks[index];
}

size_t
rir_flow_block_id(const RIRFlowBlock *block)
{
    return block != NULL ? block->block_id : 0;
}

bool
rir_flow_block_is_reachable(const RIRFlowBlock *block)
{
    return block != NULL && block->is_reachable;
}

bool
rir_flow_block_is_join(const RIRFlowBlock *block)
{
    return block != NULL && block->is_join;
}

unsigned int
rir_flow_block_entry_semantics(const RIRFlowBlock *block)
{
    return block != NULL ? block->entry_semantics : RIR_FLOW_NONE;
}

unsigned int
rir_flow_block_exit_semantics(const RIRFlowBlock *block)
{
    return block != NULL ? block->exit_semantics : RIR_FLOW_NONE;
}

size_t
rir_flow_block_fact_count(const RIRFlowBlock *block)
{
    return block != NULL ? block->fact_count : 0;
}

const RIRFlowFact *
rir_flow_block_fact_at(const RIRFlowBlock *block, size_t index)
{
    if (block == NULL || block->facts == NULL || index >= block->fact_count)
        return NULL;
    return &block->facts[index];
}

void
rir_dump(const RIRProgram *rir, FILE *out)
{
    RIRScopeInventory inventory;

    if (out == NULL)
        out = stdout;
    if (rir == NULL) {
        fprintf(out, "RIR: (null)\n");
        return;
    }

    rir_scope_inventory_from_program(rir, &inventory);
    fprintf(out, "RIR Program\n  scopes: %zu\n", inventory.count);
    for (size_t i = 0; i < inventory.count; i++) {
        const RIRScope *scope = rir_scope_inventory_get(&inventory, i);
        size_t fact_count;
        size_t op_count;
        size_t summary_count;

        if (scope == NULL)
            continue;
        fact_count = rir_scope_fact_count(scope);
        op_count = rir_scope_op_count(scope);
        summary_count = rir_scope_state_summary_count(scope);
        fprintf(out, "  scope[%02zu] %-8s %s%s%s source=%u identity=%s facts=%zu ops=%zu\n",
                i,
                rir_scope_kind_name(rir_scope_kind(scope)),
                rir_scope_owner_name(scope) != NULL ? rir_scope_owner_name(scope) : "",
                rir_scope_owner_name(scope) != NULL ? "." : "",
                rir_scope_display_name(scope),
                scope->source_syntax_id,
                scope->resource_identity_verified ? "verified" : "pending",
                fact_count,
                op_count);
        fprintf(out, "    normalize summaries=%zu state-errors=%s semantics=",
                summary_count,
                rir_scope_has_state_errors(scope) ? "yes" : "no");
        rir_dump_flow_semantics(out, rir_scope_conservative_semantics(scope));
        fputc('\n', out);
        fprintf(out,
                "    function-param-flow-summaries=%zu\n",
                rir_scope_function_param_flow_summary_count(scope));
        for (size_t j = 0;
             j < rir_scope_function_param_flow_summary_count(scope);
             j++) {
            const RIRFunctionParamFlowSummary *summary =
                rir_scope_function_param_flow_summary_at(scope, j);
            fprintf(out,
                    "    function-param-flow[%02zu] parameter-index=%zu mask=%u\n",
                    j,
                    summary != NULL ? summary->parameter_index : 0,
                    summary != NULL ? summary->mask : 0);
        }
        fprintf(out,
                "    resource-flow-symbols=%zu\n",
                rir_scope_resource_flow_symbol_count(scope));
        for (size_t j = 0;
             j < rir_scope_resource_flow_symbol_count(scope); j++) {
            const RIRResourceFlowSymbol *symbol =
                rir_scope_resource_flow_symbol_at(scope, j);
            fprintf(out,
                    "    resource-flow[%02zu] name=%s stable=%zu declaration=%u parameter=%s parameter-index=%zu\n",
                    j,
                    symbol != NULL && symbol->name != NULL
                        ? symbol->name : "-",
                    symbol != NULL ? symbol->stable_index : 0,
                    symbol != NULL ? symbol->declaration_syntax_id : 0,
                    symbol != NULL && symbol->is_parameter ? "true" : "false",
                    symbol != NULL ? symbol->parameter_index : 0);
        }
        for (size_t j = 0; j < fact_count; j++) {
            const RIRFact *fact = rir_scope_fact_at(scope, j);
            if (fact == NULL) {
                fprintf(out, "    fact[%02zu] <invalid>\n", j);
                continue;
            }
            fprintf(out, "    fact[%02zu] %-13s name=%s slot=%s arg0=%s arg1=%s kind=%s state=%s identity=%s stable=%zu\n",
                    j,
                    rir_fact_kind_name(fact->kind),
                    fact->name != NULL ? fact->name : "-",
                    fact->slot_anchor != NULL ? fact->slot_anchor : "-",
                    fact->arg0 != NULL ? fact->arg0 : "-",
                    fact->arg1 != NULL ? fact->arg1 : "-",
                    rir_resource_kind_name(fact->resource_kind),
                     rir_resource_state_name(fact->state),
                     fact->has_flow_identity ? "verified" : "missing",
                     fact->has_flow_identity ? fact->stable_index : 0);
        }
        for (size_t j = 0; j < op_count; j++) {
            const RIROp *op = rir_scope_op_at(scope, j);
            if (op == NULL) {
                fprintf(out, "    op[%02zu] <invalid>\n", j);
                continue;
            }
            fprintf(out, "    op[%02zu] %-20s subject=%s slot=%s arg0=%s arg1=%s machine=%s\n",
                    j,
                    rir_op_kind_name(op->kind),
                    op->subject != NULL ? op->subject : "-",
                    op->slot_anchor != NULL ? op->slot_anchor : "-",
                    op->arg0 != NULL ? op->arg0 : "-",
                    op->arg1 != NULL ? op->arg1 : "-",
                    rir_machine_contact_kind_name(op->machine_contact_kind));
        }
        for (size_t j = 0; j < summary_count; j++) {
            const RIRStateSummary *summary =
                rir_scope_state_summary_at(scope, j);
            if (summary == NULL) {
                fprintf(out, "    state[%02zu] <invalid>\n", j);
                continue;
            }
            fprintf(out,
                    "    state[%02zu] %-13s name=%s slot=%s kind=%s init=%s final=%s last-op=%s error=%s identity=%s stable=%zu\n",
                    j,
                    rir_fact_kind_name(summary->origin_kind),
                    summary->name != NULL ? summary->name : "-",
                    summary->slot_anchor != NULL ? summary->slot_anchor : "-",
                    rir_resource_kind_name(summary->resource_kind),
                    rir_resource_state_name(summary->initial_state),
                    rir_resource_state_name(summary->final_state),
                    summary->last_op_name != NULL ? summary->last_op_name : "-",
                     summary->has_transition_error ? "yes" : "no",
                     summary->has_flow_identity ? "verified" : "missing",
                     summary->has_flow_identity ? summary->stable_index : 0);
        }
        for (size_t j = 0; j < rir_scope_flow_block_count(scope); j++) {
            const RIRFlowBlock *block = rir_scope_flow_block_at(scope, j);
            size_t fact_count;
            if (block == NULL)
                continue;
            fact_count = rir_flow_block_fact_count(block);
            fprintf(out,
                    "    flow-block[%02zu] reachable=%s join=%s facts=%zu sem-entry=",
                    rir_flow_block_id(block),
                    rir_flow_block_is_reachable(block) ? "yes" : "no",
                    rir_flow_block_is_join(block) ? "yes" : "no",
                    fact_count);
            rir_dump_flow_semantics(out, rir_flow_block_entry_semantics(block));
            fputs(" sem-exit=", out);
            rir_dump_flow_semantics(out, rir_flow_block_exit_semantics(block));
            fputc('\n', out);
            for (size_t k = 0; k < fact_count; k++) {
                const RIRFlowFact *fact = rir_flow_block_fact_at(block, k);
                if (fact == NULL)
                    continue;
                fprintf(out,
                        "      flow[%02zu] name=%s slot=%s entry=%s exit=%s join=%s widened=%s entry-conflict=%s exit-conflict=%s\n",
                        k,
                        fact->name != NULL ? fact->name : "-",
                        fact->slot_anchor != NULL ? fact->slot_anchor : "-",
                        rir_resource_state_name(fact->entry_state),
                        rir_resource_state_name(fact->exit_state),
                        fact->merged_from_join ? "yes" : "no",
                        fact->widened_by_loop ? "yes" : "no",
                        fact->entry_conflict ? "yes" : "no",
                        fact->has_merge_conflict ? "yes" : "no");
            }
        }
    }
}
