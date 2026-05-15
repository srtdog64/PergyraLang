#include "rir.h"
#include "rir_internal.h"

#include <string.h>

#include "dir.h"
#include "rir_flow_state.h"
#include "parser/ast_api.h"
#include "../common/string_compat.h"

static const RIRFact *
rir_scope_find_projection_fact(const RIRScope *scope, const char *name)
{
    if (scope == NULL || name == NULL)
        return NULL;

    for (size_t i = 0; i < scope->fact_count; i++) {
        const RIRFact *fact = &scope->facts[i];
        if (fact->kind == RIR_FACT_PROJECTION
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
    if (error_message != NULL)
        *error_message = NULL;
    if (rir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("RIR program is null");
        return false;
    }

    for (size_t i = 0; i < rir->scope_count; i++) {
        const RIRScope *scope = &rir->scopes[i];
        for (size_t j = 0; j < scope->state_summary_count; j++) {
            const RIRStateSummary *summary = &scope->state_summaries[j];
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

        for (size_t j = 0; j < scope->fact_count; j++) {
            const RIRFact *fact = &scope->facts[j];
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

        for (size_t j = 0; j < scope->op_count; j++) {
            const RIROp *op = &scope->ops[j];
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
            for (size_t j = 0; j < scope->op_count; j++) {
                has_commit = has_commit || scope->ops[j].kind == RIR_OP_COMMIT_INTENT;
                has_abort = has_abort || scope->ops[j].kind == RIR_OP_ABORT_INTENT;
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

static bool
rir_split_qualified_name(const char *qualified_name,
                         const char **owner_name,
                         size_t *owner_len,
                         const char **local_name)
{
    const char *dot;

    if (owner_name != NULL)
        *owner_name = NULL;
    if (owner_len != NULL)
        *owner_len = 0;
    if (local_name != NULL)
        *local_name = NULL;

    if (qualified_name == NULL)
        return false;

    dot = strrchr(qualified_name, '.');
    if (dot == NULL || dot == qualified_name || dot[1] == '\0')
        return false;

    if (owner_name != NULL)
        *owner_name = qualified_name;
    if (owner_len != NULL)
        *owner_len = (size_t)(dot - qualified_name);
    if (local_name != NULL)
        *local_name = dot + 1;
    return true;
}

static bool
rir_scope_name_matches(const RIRScope *scope, const char *owner_name, size_t owner_len)
{
    return scope != NULL
        && scope->name != NULL
        && owner_name != NULL
        && strlen(scope->name) == owner_len
        && strncmp(scope->name, owner_name, owner_len) == 0;
}

static const RIRFact *
rir_scope_find_fact_by_name_kind(const RIRScope *scope, RIRFactKind kind, const char *name)
{
    if (scope == NULL || name == NULL)
        return NULL;

    for (size_t i = 0; i < scope->fact_count; i++) {
        if (scope->facts[i].kind == kind
            && scope->facts[i].name != NULL
            && strcmp(scope->facts[i].name, name) == 0) {
            return &scope->facts[i];
        }
    }

    return NULL;
}

static bool
rir_scope_has_capability_fact(const RIRScope *scope, const char *participant, const char *ability)
{
    if (scope == NULL || participant == NULL || ability == NULL)
        return false;

    for (size_t i = 0; i < scope->fact_count; i++) {
        const RIRFact *fact = &scope->facts[i];
        if (fact->kind == RIR_FACT_CAPABILITY
            && fact->name != NULL
            && strcmp(fact->name, participant) == 0
            && fact->arg0 != NULL
            && strcmp(fact->arg0, ability) == 0) {
            return true;
        }
    }

    return false;
}

static const RIRScope *
rir_find_domain_scope_for_owner(const RIRProgram *rir, const char *owner_name, size_t owner_len)
{
    for (size_t i = 0; i < rir->scope_count; i++) {
        const RIRScope *scope = &rir->scopes[i];
        if ((scope->kind == RIR_SCOPE_ZONE
             || scope->kind == RIR_SCOPE_RELATION
             || scope->kind == RIR_SCOPE_EFFECT)
            && rir_scope_name_matches(scope, owner_name, owner_len)) {
            return scope;
        }
    }

    return NULL;
}

bool
rir_validate_against_dir(const RIRProgram *rir, const DIRProgram *dir, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (rir == NULL || dir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("RIR/DIR validation requires both programs");
        return false;
    }

    for (size_t i = 0; i < dir->node_count; i++) {
        const DIRNode *node = &dir->nodes[i];
        const char *owner_name = NULL;
        const char *local_name = NULL;
        size_t owner_len = 0;
        const RIRScope *scope;
        const RIRFact *fact;
        const RIRStateSummary *summary;
        RIRResourceKind expected_kind = RIR_RESOURCE_UNKNOWN;

        if (node->kind != DIR_NODE_ZONE_SLOT
            && node->kind != DIR_NODE_PROJECTION_SLOT
            && node->kind != DIR_NODE_AUTHORITY_SLOT) {
            continue;
        }

        if (!rir_split_qualified_name(node->name, &owner_name, &owner_len, &local_name)) {
            if (error_message != NULL) {
                *error_message = rir_strdup_fmt(
                    "DIR slot-contract node '%s' is not owner-qualified",
                    node->name != NULL ? node->name : "(unnamed)");
            }
            return false;
        }

        scope = rir_find_domain_scope_for_owner(rir, owner_name, owner_len);
        if (scope == NULL) {
            if (error_message != NULL) {
                *error_message = rir_strdup_fmt(
                    "DIR slot-contract '%s' has no matching RIR domain scope",
                    node->name != NULL ? node->name : "(unnamed)");
            }
            return false;
        }

        if (node->kind == DIR_NODE_AUTHORITY_SLOT) {
            fact = rir_scope_find_fact_by_name_kind(scope, RIR_FACT_AUTHORITY, local_name);
            if (fact == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' is missing authority fact for DIR authority-slot '%s'",
                        scope->name != NULL ? scope->name : "(anonymous)",
                        node->name != NULL ? node->name : "(unnamed)");
                }
                return false;
            }

            for (size_t edge_i = 0; edge_i < dir->edge_count; edge_i++) {
                const DIREdge *edge = &dir->edges[edge_i];
                if (edge->kind != DIR_EDGE_ZONE_AUTHORITY_ABILITY
                    || edge->from_node_id != node->id
                    || edge->target_name == NULL) {
                    continue;
                }
                if (!rir_scope_has_capability_fact(scope, local_name, edge->target_name)) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' is missing capability fact '%s' for DIR authority-slot '%s'",
                            scope->name != NULL ? scope->name : "(anonymous)",
                            edge->target_name,
                            node->name != NULL ? node->name : "(unnamed)");
                    }
                    return false;
                }
            }
            continue;
        }

        fact = rir_scope_find_fact_by_name_kind(scope, RIR_FACT_RESOURCE, local_name);
        summary = scope_find_state_summary((RIRScope *)scope, local_name);

        if (node->ast != NULL && node->ast->type == AST_DOMAIN_SLOT) {
            if (ast_domain_slot_is_subject(node->ast))
                expected_kind = RIR_RESOURCE_SUBJECT_SLOT;
            else if (ast_domain_slot_is_vessel(node->ast))
                expected_kind = RIR_RESOURCE_VESSEL_SLOT;
            else if (ast_domain_slot_is_tobject(node->ast))
                expected_kind = RIR_RESOURCE_TOBJECT_SLOT;
            else
                expected_kind = RIR_RESOURCE_OBJECT_SLOT;
        }

        if (fact == NULL || summary == NULL) {
            if (error_message != NULL) {
                *error_message = rir_strdup_fmt(
                    "RIR scope '%s' is missing resource fact/state for DIR slot-contract '%s'",
                    scope->name != NULL ? scope->name : "(anonymous)",
                    node->name != NULL ? node->name : "(unnamed)");
            }
            return false;
        }

        if (expected_kind != RIR_RESOURCE_UNKNOWN
            && (fact->resource_kind != expected_kind || summary->resource_kind != expected_kind)) {
            if (error_message != NULL) {
                *error_message = rir_strdup_fmt(
                    "RIR scope '%s' kind mismatch for DIR slot-contract '%s'",
                    scope->name != NULL ? scope->name : "(anonymous)",
                    node->name != NULL ? node->name : "(unnamed)");
            }
            return false;
        }

        if (node->kind == DIR_NODE_PROJECTION_SLOT) {
            const RIRFact *projection = rir_scope_find_projection_fact(scope, local_name);
            bool has_projection_source = false;
            for (size_t edge_i = 0; edge_i < dir->edge_count; edge_i++) {
                const DIREdge *edge = &dir->edges[edge_i];
                if (edge->kind == DIR_EDGE_PROJECTION_SLOT_SOURCE
                    && edge->from_node_id == node->id) {
                    has_projection_source = true;
                    break;
                }
            }
            if (has_projection_source && projection == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' is missing projection fact for DIR projection-slot '%s'.\n"
                        "Reason:\n"
                        "- DIR declared a projection slot/source edge for '%s'\n"
                        "- RIR lowering did not materialize the corresponding projection fact\n"
                        "Fix:\n"
                        "- lower DIR projection-slot contracts into RIR facts before validation\n"
                        "- or remove the stale DIR projection edge",
                        scope->name != NULL ? scope->name : "(anonymous)",
                        node->name != NULL ? node->name : "(unnamed)",
                        node->name != NULL ? node->name : "(unnamed)");
                }
                return false;
            }
        }
    }

    return true;
}

/* =================================================================
 * JSON Export — RIR Facts for AI/External Consumption
 *
 * This function dumps the RIR program as structured JSON.
 * The output is consumed by:
 *   1. AI/MCP agents that map RIR Facts → MIR Instructions
 *   2. External tooling that validates resource ownership
 *   3. Debug/audit logs for intent orchestration
 *
 * Schema: see docs/39_test_driven_abi_and_explicit_lowering.md §2.2
 * ================================================================= */

