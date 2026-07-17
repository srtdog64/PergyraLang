#include "rir.h"
#include "rir_internal.h"

#include <string.h>

#include "rir_flow_state.h"
#include "parser/ast_api.h"
#include "../common/string_compat.h"

static RIRResourceKind
rir_direct_projection_kind_from_ast(const ASTNode *ast)
{
    if (ast == NULL || ast->type != AST_CALL
        || ast_call_callee(ast) == NULL
        || ast_call_callee(ast)->type != AST_IDENTIFIER
        || ast_identifier_name(ast_call_callee(ast)) == NULL) {
        return RIR_RESOURCE_UNKNOWN;
    }

    if (strcmp(ast_identifier_name(ast_call_callee(ast)), "ToObject") == 0)
        return RIR_RESOURCE_PROJECTION_OBJECT;
    if (strcmp(ast_identifier_name(ast_call_callee(ast)), "ToTObject") == 0)
        return RIR_RESOURCE_PROJECTION_TOBJECT;
    return RIR_RESOURCE_UNKNOWN;
}

static bool
rir_validate_resource_flow_symbols(const RIRScope *scope,
                                   char **error_message)
{
    size_t count;

    if (scope == NULL)
        return false;
    count = scope->resource_flow_symbol_count;
    if (count == 0) {
        if (scope->resource_flow_symbols != NULL
            || scope->resource_flow_symbol_capacity != 0) {
            if (error_message != NULL)
                *error_message = rir_strdup_fmt(
                    "RIR scope '%s' has ResourceFlow storage without rows",
                    rir_scope_display_name(scope));
            return false;
        }
        return true;
    }
    if (scope->resource_flow_symbols == NULL
        || count > scope->resource_flow_symbol_capacity) {
        if (error_message != NULL)
            *error_message = rir_strdup_fmt(
                "RIR scope '%s' has incomplete ResourceFlow symbol storage",
                rir_scope_display_name(scope));
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        const RIRResourceFlowSymbol *symbol =
            &scope->resource_flow_symbols[i];
        if (symbol->name == NULL || symbol->name[0] == '\0') {
            if (error_message != NULL)
                *error_message = rir_strdup_fmt(
                    "RIR scope '%s' ResourceFlow symbol[%llu] has no name",
                    rir_scope_display_name(scope),
                    (unsigned long long)i);
            return false;
        }
        for (size_t j = 0; j < i; j++) {
            const RIRResourceFlowSymbol *prior =
                &scope->resource_flow_symbols[j];
            if (prior->stable_index == symbol->stable_index
                || (symbol->is_parameter && prior->is_parameter
                    && prior->parameter_index == symbol->parameter_index)) {
                if (error_message != NULL)
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' ResourceFlow symbols share identity",
                        rir_scope_display_name(scope));
                return false;
            }
        }
    }
    return true;
}

static bool
rir_scope_resource_flow_index_known(const RIRScope *scope,
                                    size_t stable_index)
{
    if (scope == NULL)
        return false;
    for (size_t i = 0; i < scope->resource_flow_symbol_count; i++) {
        if (scope->resource_flow_symbols[i].stable_index == stable_index)
            return true;
    }
    return false;
}

RIRResourceState
rir_merge_state_for_kind(RIRResourceKind kind,
                         RIRResourceState a,
                         RIRResourceState b,
                         bool *conflict)
{
    if (conflict != NULL)
        *conflict = false;
    return rir_merge_states_for_kind(kind, a, b, conflict);
}

bool
rir_validate(const RIRProgram *rir, char **error_message)
{
    RIRScopeInventory inventory;
    if (error_message != NULL)
        *error_message = NULL;
    if (rir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("RIR program is null");
        return false;
    }

    rir_scope_inventory_from_program(rir, &inventory);
    for (size_t i = 0; i < inventory.count; i++) {
        const RIRScope *scope = rir_scope_inventory_get(&inventory, i);
        if (scope == NULL) {
            if (error_message != NULL)
                *error_message =
                    rir_strdup_fmt("RIR scope inventory row[%llu] is invalid",
                                   (unsigned long long)i);
            return false;
        }
        if (!rir_validate_resource_flow_symbols(scope, error_message))
            return false;
        for (size_t j = 0; j < rir_scope_state_summary_count(scope); j++) {
            const char *scope_name = rir_scope_display_name(scope);
            const RIRStateSummary *summary =
                rir_scope_state_summary_at(scope, j);
            if (summary == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' has invalid state summary[%llu]",
                        scope_name,
                        (unsigned long long) j);
                }
                return false;
            }
            if (summary->name == NULL || summary->resource_kind == RIR_RESOURCE_UNKNOWN) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' has incomplete state summary[%llu]",
                        scope_name,
                        (unsigned long long) j);
                }
                return false;
            }
            if (summary->slot_anchor == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' state summary '%s' is missing slot anchor",
                        scope_name,
                        summary->name != NULL ? summary->name : "(unnamed)");
                }
                return false;
            }
            if (scope->resource_identity_verified
                && summary->origin_kind == RIR_FACT_RESOURCE
                && !summary->has_flow_identity) {
                if (error_message != NULL)
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' state summary '%s' is missing HIR stable identity",
                        scope_name,
                        summary->name != NULL ? summary->name : "(unnamed)");
                return false;
            }
            if (scope->resource_identity_verified
                && summary->origin_kind == RIR_FACT_RESOURCE
                && !rir_scope_resource_flow_index_known(
                    scope, summary->stable_index)) {
                if (error_message != NULL)
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' state summary '%s' references unknown ResourceFlow identity",
                        scope_name,
                        summary->name != NULL ? summary->name : "(unnamed)");
                return false;
            }
        }

        if (rir_scope_function_param_flow_summary_count(scope) > 0) {
            if (scope->function_param_flow_summaries == NULL
                || scope->function_param_flow_summary_count
                    > scope->function_param_flow_summary_capacity
                || (scope->kind != RIR_SCOPE_FUNCTION
                    && scope->kind != RIR_SCOPE_METHOD)) {
                if (error_message != NULL)
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' has incomplete function parameter flow summary storage",
                        rir_scope_display_name(scope),
                        0);
                return false;
            }
            for (size_t j = 0;
                 j < rir_scope_function_param_flow_summary_count(scope);
                 j++) {
                const RIRFunctionParamFlowSummary *summary =
                    rir_scope_function_param_flow_summary_at(scope, j);
                if (summary == NULL
                    || summary->parameter_index >= scope->parameter_count) {
                    if (error_message != NULL)
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' has invalid function parameter flow summary[%llu]",
                            rir_scope_display_name(scope),
                            (unsigned long long)j);
                    return false;
                }
                for (size_t k = 0; k < j; k++) {
                    const RIRFunctionParamFlowSummary *prior =
                        rir_scope_function_param_flow_summary_at(scope, k);
                    if (prior != NULL
                        && prior->parameter_index == summary->parameter_index) {
                        if (error_message != NULL)
                            *error_message = rir_strdup_fmt(
                                "RIR scope '%s' function parameter flow summaries share parameter identity",
                                rir_scope_display_name(scope),
                                (unsigned long long)summary->parameter_index);
                        return false;
                    }
                }
            }
        }

        for (size_t j = 0; j < rir_scope_fact_count(scope); j++) {
            const char *scope_name = rir_scope_display_name(scope);
            const RIRFact *fact = rir_scope_fact_at(scope, j);
            if (fact == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' has invalid fact[%llu]",
                        scope_name,
                        (unsigned long long) j);
                }
                return false;
            }
            if (fact->kind != RIR_FACT_INTENT_POLICY && fact->slot_anchor == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' fact '%s' is missing slot anchor",
                        scope_name,
                        fact->name != NULL ? fact->name : "(unnamed)");
                }
                return false;
            }
            if (scope->resource_identity_verified
                && fact->kind == RIR_FACT_RESOURCE
                && !fact->has_flow_identity) {
                if (error_message != NULL)
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' resource fact '%s' is missing HIR stable identity",
                        scope_name,
                        fact->name != NULL ? fact->name : "(unnamed)");
                return false;
            }
            if (scope->resource_identity_verified
                && fact->kind == RIR_FACT_RESOURCE
                && !rir_scope_resource_flow_index_known(
                    scope, fact->stable_index)) {
                if (error_message != NULL)
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' resource fact '%s' references unknown ResourceFlow identity",
                        scope_name,
                        fact->name != NULL ? fact->name : "(unnamed)");
                return false;
            }
            if (fact->kind == RIR_FACT_PROJECTION) {
                if (fact->resource_kind == RIR_RESOURCE_PROJECTION_OBJECT
                    && fact->state == RIR_STATE_PUBLISHED) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' object projection '%s' cannot be Published.\n"
                            "Reason:\n"
                            "- object projections are local refresh targets\n"
                            "- published state is reserved for boundary tobject projections\n"
                            "Fix:\n"
                            "- keep '%s' in refresh/bind flow\n"
                            "- or change it to a tobject projection before publish",
                            scope_name,
                            fact->name != NULL ? fact->name : "(unnamed)",
                            fact->name != NULL ? fact->name : "(unnamed)");
                    }
                    return false;
                }
                if (fact->resource_kind == RIR_RESOURCE_PROJECTION_TOBJECT
                    && strcmp(fact->arg1 != NULL ? fact->arg1 : "", "refresh") == 0) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' tobject projection '%s' cannot use refresh mode.\n"
                            "Reason:\n"
                            "- tobject projections are boundary publish targets\n"
                            "- refresh mode is only valid for local object projections\n"
                            "Fix:\n"
                            "- use publish for '%s'\n"
                            "- or switch the target declaration to object",
                            scope_name,
                            fact->name != NULL ? fact->name : "(unnamed)",
                            fact->name != NULL ? fact->name : "(unnamed)");
                    }
                    return false;
                }
            }
        }

        for (size_t j = 0; j < rir_scope_op_count(scope); j++) {
            const char *scope_name = rir_scope_display_name(scope);
            const RIROp *op = rir_scope_op_at(scope, j);
            if (op == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' has invalid op[%llu]",
                        scope_name,
                        (unsigned long long) j);
                }
                return false;
            }
            if (op->slot_anchor == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' op '%s' is missing slot anchor",
                        scope_name,
                        rir_op_kind_name(op->kind));
                }
                return false;
            }
            if (op->kind == RIR_OP_PROJECT_PUBLISH || op->kind == RIR_OP_PROJECT_REFRESH) {
                const RIRFact *projection = rir_scope_find_projection_fact(scope, op->slot_anchor);
                const RIRStateSummary *summary =
                    rir_scope_find_state_summary(scope, op->slot_anchor);
                RIRResourceKind kind = RIR_RESOURCE_UNKNOWN;

                if (projection != NULL)
                    kind = projection->resource_kind;
                else if (summary != NULL)
                    kind = summary->resource_kind;
                else
                    kind = rir_direct_projection_kind_from_ast(op->ast);

                if (kind == RIR_RESOURCE_UNKNOWN) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' %s on '%s' has unknown projection kind.\n"
                            "Reason:\n"
                            "- the projection op reached validation without a projection fact or state summary\n"
                            "- backend lowering would otherwise guess the projection contract\n"
                            "Fix:\n"
                            "- materialize projection facts before validation\n"
                            "- or ensure DIR projection-slot edges are lowered into RIR summaries",
                            scope_name,
                            op->kind == RIR_OP_PROJECT_PUBLISH ? "ProjectPublish" : "ProjectRefresh",
                            op->slot_anchor);
                    }
                    return false;
                }

                if (op->kind == RIR_OP_PROJECT_PUBLISH
                    && kind != RIR_RESOURCE_PROJECTION_TOBJECT
                    && kind != RIR_RESOURCE_TOBJECT_SLOT) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' ProjectPublish requires tobject projection slot '%s'.\n"
                            "Reason:\n"
                            "- publish emits a boundary transfer snapshot\n"
                            "- object/local projection slots cannot satisfy publish semantics\n"
                            "Fix:\n"
                            "- target a tobject projection slot\n"
                            "- or use ProjectRefresh for local object projection sync",
                            scope_name,
                            op->slot_anchor);
                    }
                    return false;
                }
                if (op->kind == RIR_OP_PROJECT_REFRESH
                    && kind != RIR_RESOURCE_PROJECTION_OBJECT
                    && kind != RIR_RESOURCE_OBJECT_SLOT) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' ProjectRefresh requires object projection slot '%s'.\n"
                            "Reason:\n"
                            "- refresh updates a local object projection from a source anchor\n"
                            "- tobject/boundary slots require publish semantics instead\n"
                            "Fix:\n"
                            "- target an object projection slot\n"
                            "- or switch this op to ProjectPublish for tobject flow",
                            scope_name,
                            op->slot_anchor);
                    }
                    return false;
                }
            }
        }

        for (size_t j = 0; j < rir_scope_flow_block_count(scope); j++) {
            const char *scope_name = rir_scope_display_name(scope);
            const RIRFlowBlock *block = rir_scope_flow_block_at(scope, j);
            size_t flow_fact_count;
            if (block == NULL)
                continue;
            flow_fact_count = rir_flow_block_fact_count(block);
            if (flow_fact_count > 0 && rir_flow_block_fact_at(block, 0) == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' flow-block[%llu] has missing fact storage",
                        scope_name,
                        (unsigned long long) rir_flow_block_id(block));
                }
                return false;
            }
            for (size_t k = 0; k < flow_fact_count; k++) {
                const RIRFlowFact *fact = rir_flow_block_fact_at(block, k);
                if (fact == NULL)
                    continue;
                if (fact->name == NULL) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' flow-block[%llu] has unnamed fact",
                            scope_name,
                            (unsigned long long) rir_flow_block_id(block));
                    }
                    return false;
                }
                if (fact->slot_anchor == NULL) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' flow-block[%llu] fact '%s' is missing slot anchor",
                            scope_name,
                            (unsigned long long) rir_flow_block_id(block),
                            fact->name != NULL ? fact->name : "(unnamed)");
                    }
                    return false;
                }
            }
        }

        if (rir_scope_kind(scope) == RIR_SCOPE_INTENT) {
            bool has_commit = false;
            bool has_abort = false;
            for (size_t j = 0; j < rir_scope_op_count(scope); j++) {
                const RIROp *op = rir_scope_op_at(scope, j);
                if (op == NULL)
                    continue;
                has_commit = has_commit || op->kind == RIR_OP_COMMIT_INTENT;
                has_abort = has_abort || op->kind == RIR_OP_ABORT_INTENT;
            }
            if (!has_commit || !has_abort) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR intent scope '%s' is missing commit/abort structure",
                        rir_scope_display_name(scope));
                }
                return false;
            }
        }
    }

    return true;
}

