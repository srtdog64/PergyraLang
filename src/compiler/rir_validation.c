#include "rir.h"
#include "rir_internal.h"

#include <string.h>

#include "rir_flow_state.h"
#include "parser/ast_api.h"
#include "../common/string_compat.h"

const RIRFact *
rir_scope_find_projection_fact(const RIRScope *scope, const char *name)
{
    if (scope == NULL || name == NULL)
        return NULL;

    for (size_t i = 0; i < rir_scope_fact_count(scope); i++) {
        const RIRFact *fact = rir_scope_fact_at(scope, i);
        if (fact != NULL
            && fact->kind == RIR_FACT_PROJECTION
            && fact->name != NULL
            && strcmp(fact->name, name) == 0) {
            return fact;
        }
    }

    return NULL;
}

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
        for (size_t j = 0; j < rir_scope_state_summary_count(scope); j++) {
            const RIRStateSummary *summary =
                rir_scope_state_summary_at(scope, j);
            if (summary == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' has invalid state summary[%llu]",
                        scope->name != NULL ? scope->name : "(anonymous)",
                        (unsigned long long) j);
                }
                return false;
            }
            if (summary->name == NULL || summary->resource_kind == RIR_RESOURCE_UNKNOWN) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' has incomplete state summary[%llu]",
                        scope->name != NULL ? scope->name : "(anonymous)",
                        (unsigned long long) j);
                }
                return false;
            }
            if (summary->slot_anchor == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' state summary '%s' is missing slot anchor",
                        scope->name != NULL ? scope->name : "(anonymous)",
                        summary->name != NULL ? summary->name : "(unnamed)");
                }
                return false;
            }
        }

        for (size_t j = 0; j < rir_scope_fact_count(scope); j++) {
            const RIRFact *fact = rir_scope_fact_at(scope, j);
            if (fact == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' has invalid fact[%llu]",
                        scope->name != NULL ? scope->name : "(anonymous)",
                        (unsigned long long) j);
                }
                return false;
            }
            if (fact->kind != RIR_FACT_INTENT_POLICY && fact->slot_anchor == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' fact '%s' is missing slot anchor",
                        scope->name != NULL ? scope->name : "(anonymous)",
                        fact->name != NULL ? fact->name : "(unnamed)");
                }
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
                            scope->name != NULL ? scope->name : "(anonymous)",
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
                            scope->name != NULL ? scope->name : "(anonymous)",
                            fact->name != NULL ? fact->name : "(unnamed)",
                            fact->name != NULL ? fact->name : "(unnamed)");
                    }
                    return false;
                }
            }
        }

        for (size_t j = 0; j < rir_scope_op_count(scope); j++) {
            const RIROp *op = rir_scope_op_at(scope, j);
            if (op == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' has invalid op[%llu]",
                        scope->name != NULL ? scope->name : "(anonymous)",
                        (unsigned long long) j);
                }
                return false;
            }
            if (op->slot_anchor == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' op '%s' is missing slot anchor",
                        scope->name != NULL ? scope->name : "(anonymous)",
                        rir_op_kind_name(op->kind));
                }
                return false;
            }
            if (op->kind == RIR_OP_PROJECT_PUBLISH || op->kind == RIR_OP_PROJECT_REFRESH) {
                const RIRFact *projection = rir_scope_find_projection_fact(scope, op->slot_anchor);
                const RIRStateSummary *summary = scope_find_state_summary(
                    (RIRScope *)scope, op->slot_anchor);
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
                            scope->name != NULL ? scope->name : "(anonymous)",
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
                            scope->name != NULL ? scope->name : "(anonymous)",
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
                            scope->name != NULL ? scope->name : "(anonymous)",
                            op->slot_anchor);
                    }
                    return false;
                }
            }
        }

        for (size_t j = 0; j < scope->flow_block_count; j++) {
            const RIRFlowBlock *block = &scope->flow_blocks[j];
            if (block->fact_count > 0 && block->facts == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' flow-block[%llu] has missing fact storage",
                        scope->name != NULL ? scope->name : "(anonymous)",
                        (unsigned long long) block->block_id);
                }
                return false;
            }
            for (size_t k = 0; k < block->fact_count; k++) {
                if (block->facts[k].name == NULL) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' flow-block[%llu] has unnamed fact",
                            scope->name != NULL ? scope->name : "(anonymous)",
                            (unsigned long long) block->block_id);
                    }
                    return false;
                }
                if (block->facts[k].slot_anchor == NULL) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' flow-block[%llu] fact '%s' is missing slot anchor",
                            scope->name != NULL ? scope->name : "(anonymous)",
                            (unsigned long long) block->block_id,
                            block->facts[k].name != NULL ? block->facts[k].name : "(unnamed)");
                    }
                    return false;
                }
            }
        }

        if (scope->kind == RIR_SCOPE_INTENT) {
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
                        scope->name != NULL ? scope->name : "(anonymous)");
                }
                return false;
            }
        }
    }

    return true;
}

