#include "rir_internal.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool
rir_scope_next_capacity(size_t current, size_t item_size, size_t *out)
{
    size_t next_capacity;

    if (out == NULL)
        return false;
    if (current == 0) {
        next_capacity = 8;
    } else {
        if (current > SIZE_MAX / 2)
            return false;
        next_capacity = current * 2;
    }
    if (next_capacity > SIZE_MAX / item_size)
        return false;
    *out = next_capacity;
    return true;
}

bool
append_scope(RIRProgram *rir, RIRScope scope)
{
    if (rir->scope_count == rir->scope_capacity) {
        size_t next_capacity;
        RIRScope *grown;

        if (!rir_scope_next_capacity(rir->scope_capacity,
                                     sizeof(RIRScope),
                                     &next_capacity)) {
            return false;
        }
        grown = realloc(rir->scopes, next_capacity * sizeof(RIRScope));
        if (grown == NULL)
            return false;
        rir->scopes = grown;
        rir->scope_capacity = next_capacity;
    }
    rir->scopes[rir->scope_count] = scope;
    rir->scope_count++;
    return true;
}

char *
rir_strdup_fmt(const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int length;
    char *result;

    va_start(args, fmt);
    va_copy(copy, args);
    length = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (length < 0) {
        va_end(args);
        return NULL;
    }

    result = malloc((size_t)length + 1);
    if (result == NULL) {
        va_end(args);
        return NULL;
    }
    vsnprintf(result, (size_t)length + 1, fmt, args);
    va_end(args);
    return result;
}

bool
scope_add_fact(RIRScope *scope, RIRFact fact)
{
    if (scope->fact_count == scope->fact_capacity) {
        size_t next_capacity;
        RIRFact *grown;

        if (!rir_scope_next_capacity(scope->fact_capacity,
                                     sizeof(RIRFact),
                                     &next_capacity)) {
            return false;
        }
        grown = realloc(scope->facts, next_capacity * sizeof(RIRFact));
        if (grown == NULL)
            return false;
        scope->facts = grown;
        scope->fact_capacity = next_capacity;
    }
    scope->facts[scope->fact_count] = fact;
    scope->fact_count++;
    return true;
}

bool
scope_add_op(RIRScope *scope, RIROp op)
{
    if (scope->op_count == scope->op_capacity) {
        size_t next_capacity;
        RIROp *grown;

        if (!rir_scope_next_capacity(scope->op_capacity,
                                     sizeof(RIROp),
                                     &next_capacity)) {
            return false;
        }
        grown = realloc(scope->ops, next_capacity * sizeof(RIROp));
        if (grown == NULL)
            return false;
        scope->ops = grown;
        scope->op_capacity = next_capacity;
    }
    scope->ops[scope->op_count] = op;
    scope->op_count++;
    return true;
}

void
rir_free_flow_blocks(RIRScope *scope)
{
    if (scope == NULL)
        return;
    for (size_t i = 0; i < scope->flow_block_count; i++)
        free(scope->flow_blocks[i].facts);
    free(scope->flow_blocks);
    scope->flow_blocks = NULL;
    scope->flow_block_count = 0;
    scope->has_flow_sensitive_merge = false;
}

void
rir_scope_discard_storage(RIRScope *scope)
{
    if (scope == NULL)
        return;
    free(scope->facts);
    free(scope->ops);
    free(scope->state_summaries);
    rir_free_flow_blocks(scope);
    scope->facts = NULL;
    scope->fact_count = 0;
    scope->fact_capacity = 0;
    scope->ops = NULL;
    scope->op_count = 0;
    scope->op_capacity = 0;
    scope->state_summaries = NULL;
    scope->state_summary_count = 0;
    scope->state_summary_capacity = 0;
}

void
rir_scope_take_ops_and_discard(RIRScope *scope,
                               RIROp **ops_out,
                               size_t *op_count_out)
{
    if (ops_out == NULL || op_count_out == NULL)
        return;
    *ops_out = NULL;
    *op_count_out = 0;
    if (scope == NULL)
        return;
    *ops_out = scope->ops;
    *op_count_out = scope->op_count;
    scope->ops = NULL;
    scope->op_count = 0;
    scope->op_capacity = 0;
    rir_scope_discard_storage(scope);
}

static bool
scope_add_state_summary(RIRScope *scope, RIRStateSummary summary)
{
    if (scope->state_summary_count == scope->state_summary_capacity) {
        size_t next_capacity;
        RIRStateSummary *grown;

        if (!rir_scope_next_capacity(scope->state_summary_capacity,
                                     sizeof(RIRStateSummary),
                                     &next_capacity)) {
            return false;
        }
        grown = realloc(scope->state_summaries,
                        next_capacity * sizeof(RIRStateSummary));
        if (grown == NULL)
            return false;
        scope->state_summaries = grown;
        scope->state_summary_capacity = next_capacity;
    }
    scope->state_summaries[scope->state_summary_count] = summary;
    scope->state_summary_count++;
    return true;
}

RIRStateSummary *
scope_find_state_summary(RIRScope *scope, const char *name)
{
    if (scope == NULL || name == NULL)
        return NULL;

    for (size_t i = 0; i < scope->state_summary_count; i++) {
        if (scope->state_summaries[i].name != NULL
            && strcmp(scope->state_summaries[i].name, name) == 0) {
            return &scope->state_summaries[i];
        }
    }
    return NULL;
}

bool
scope_ensure_state_summary(RIRScope *scope, const RIRFact *fact)
{
    RIRStateSummary summary;
    if (scope == NULL || fact == NULL || fact->name == NULL)
        return true;
    if (fact->kind != RIR_FACT_RESOURCE
        && fact->kind != RIR_FACT_PROJECTION
        && fact->kind != RIR_FACT_AUTHORITY
        && fact->kind != RIR_FACT_CAPABILITY)
        return true;
    if (scope_find_state_summary(scope, fact->name) != NULL)
        return true;

    memset(&summary, 0, sizeof(summary));
    summary.name = fact->name;
    summary.slot_anchor = fact->slot_anchor;
    summary.has_flow_identity = fact->has_flow_identity;
    summary.stable_index = fact->stable_index;
    summary.declaration_syntax_id = fact->declaration_syntax_id;
    summary.is_parameter = fact->is_parameter;
    summary.parameter_index = fact->parameter_index;
    summary.origin_kind = fact->kind;
    summary.resource_kind = fact->resource_kind;
    summary.initial_state = fact->state;
    summary.final_state = fact->state;
    summary.ast = fact->ast;
    return scope_add_state_summary(scope, summary);
}
