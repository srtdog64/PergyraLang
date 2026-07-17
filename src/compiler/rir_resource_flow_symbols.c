#include "rir_resource_flow_symbols.h"

#include "rir_internal.h"

#include "../common/string_compat.h"

#include <stdlib.h>

static void
rir_clear_resource_flow_symbols(RIRScope *scope)
{
    if (scope == NULL)
        return;
    for (size_t i = 0; i < scope->resource_flow_symbol_count; i++)
        free(scope->resource_flow_symbols[i].name);
    free(scope->resource_flow_symbols);
    scope->resource_flow_symbols = NULL;
    scope->resource_flow_symbol_count = 0;
    scope->resource_flow_symbol_capacity = 0;
}

bool
rir_copy_resource_flow_symbols(RIRScope *scope,
                               const HIRRoutine *hir_routine,
                               char **error_message)
{
    size_t count;

    if (scope == NULL || hir_routine == NULL)
        return false;
    count = hir_routine->resource_flow_symbol_count;
    rir_clear_resource_flow_symbols(scope);
    if (count == 0)
        return true;
    if (hir_routine->resource_flow_symbols == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "RIR ResourceFlowUniverse has incomplete HIR storage");
        return false;
    }
    scope->resource_flow_symbols = calloc(
        count, sizeof(*scope->resource_flow_symbols));
    if (scope->resource_flow_symbols == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("out of memory");
        return false;
    }
    scope->resource_flow_symbol_capacity = count;
    for (size_t i = 0; i < count; i++) {
        const HIRResourceFlowSymbol *source =
            &hir_routine->resource_flow_symbols[i];
        RIRResourceFlowSymbol *target = &scope->resource_flow_symbols[i];
        if (source->name == NULL || source->name[0] == '\0') {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "RIR ResourceFlowUniverse row has no name");
            goto fail;
        }
        for (size_t j = 0; j < i; j++) {
            const RIRResourceFlowSymbol *prior =
                &scope->resource_flow_symbols[j];
            if (prior->stable_index == source->stable_index
                || (source->is_parameter && prior->is_parameter
                    && prior->parameter_index == source->parameter_index)) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "RIR ResourceFlowUniverse rows have duplicate identity");
                goto fail;
            }
        }
        *target = (RIRResourceFlowSymbol){
            .stable_index = source->stable_index,
            .declaration_syntax_id = source->declaration_syntax_id,
            .line = source->line,
            .column = source->column,
            .symbol_kind = source->symbol_kind,
            .is_parameter = source->is_parameter,
            .parameter_index = source->parameter_index,
            .name = pergyra_strdup(source->name)
        };
        if (target->name == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            goto fail;
        }
    }
    scope->resource_flow_symbol_count = count;
    return true;

fail:
    rir_clear_resource_flow_symbols(scope);
    return false;
}
