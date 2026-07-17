#include "rir.h"
#include "rir_internal.h"

#include <string.h>

#include "dir.h"
#include "parser/ast_api.h"
#include "../common/string_compat.h"

static const RIRScope *
rir_find_domain_scope_for_source_id(const RIRProgram *rir,
                                    uint32_t owner_source_syntax_id)
{
    RIRScopeInventory inventory;
    rir_scope_inventory_from_program(rir, &inventory);
    if (owner_source_syntax_id == 0)
        return NULL;
    for (size_t i = 0; i < inventory.count; i++) {
        const RIRScope *scope = rir_scope_inventory_get(&inventory, i);
        if (scope == NULL)
            continue;
        if ((rir_scope_kind(scope) == RIR_SCOPE_ZONE
             || rir_scope_kind(scope) == RIR_SCOPE_RELATION
             || rir_scope_kind(scope) == RIR_SCOPE_EFFECT)
            && scope->source_syntax_id == owner_source_syntax_id) {
            return scope;
        }
    }

    return NULL;
}

bool
rir_validate_against_dir(const RIRProgram *rir,
                         const DIRProgram *dir,
                         char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (rir == NULL || dir == NULL) {
        if (error_message != NULL)
            *error_message =
                pergyra_strdup("RIR/DIR validation requires both programs");
        return false;
    }

    for (size_t i = 0; i < dir->node_count; i++) {
        const DIRNode *node = &dir->nodes[i];
        const char *local_name = NULL;
        const RIRScope *scope;
        const RIRFact *fact;
        const RIRStateSummary *summary;
        RIRResourceKind expected_kind = RIR_RESOURCE_UNKNOWN;

        if (node->kind != DIR_NODE_ZONE_SLOT
            && node->kind != DIR_NODE_PROJECTION_SLOT
            && node->kind != DIR_NODE_AUTHORITY_SLOT) {
            continue;
        }

        if (node->name == NULL || node->owner_source_syntax_id == 0) {
            if (error_message != NULL) {
                *error_message = rir_strdup_fmt(
                    "DIR slot-contract node '%s' is missing owner source identity",
                    node->name != NULL ? node->name : "(unnamed)");
            }
            return false;
        }

        local_name = strrchr(node->name, '.');
        if (local_name == NULL || local_name[1] == '\0') {
            if (error_message != NULL) {
                *error_message = rir_strdup_fmt(
                    "DIR slot-contract node '%s' is not owner-qualified",
                    node->name);
            }
            return false;
        }
        local_name++;

        scope = rir_find_domain_scope_for_source_id(
            rir, node->owner_source_syntax_id);
        if (scope == NULL) {
            if (error_message != NULL) {
                *error_message = rir_strdup_fmt(
                    "DIR slot-contract '%s' has no RIR domain scope for owner source identity %u",
                    node->name,
                    (unsigned)node->owner_source_syntax_id);
            }
            return false;
        }

        if (node->kind == DIR_NODE_AUTHORITY_SLOT) {
            const char *scope_name = rir_scope_display_name(scope);
            fact = rir_scope_find_fact_by_name_kind(scope,
                                                    RIR_FACT_AUTHORITY,
                                                    local_name);
            if (fact == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' is missing authority fact for DIR authority-slot '%s'",
                        scope_name,
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
                if (!rir_scope_has_capability_fact(scope,
                                                   local_name,
                                                   edge->target_name)) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' is missing capability fact '%s' for DIR authority-slot '%s'",
                            scope_name,
                            edge->target_name,
                            node->name != NULL ? node->name : "(unnamed)");
                    }
                    return false;
                }
            }
            continue;
        }

        fact = rir_scope_find_fact_by_name_kind(scope,
                                                RIR_FACT_RESOURCE,
                                                local_name);
        summary = rir_scope_find_state_summary(scope, local_name);

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
                    rir_scope_display_name(scope),
                    node->name != NULL ? node->name : "(unnamed)");
            }
            return false;
        }

        if (expected_kind != RIR_RESOURCE_UNKNOWN
            && (fact->resource_kind != expected_kind
                || summary->resource_kind != expected_kind)) {
            if (error_message != NULL) {
                *error_message = rir_strdup_fmt(
                    "RIR scope '%s' kind mismatch for DIR slot-contract '%s'",
                    rir_scope_display_name(scope),
                    node->name != NULL ? node->name : "(unnamed)");
            }
            return false;
        }

        if (node->kind == DIR_NODE_PROJECTION_SLOT) {
            const RIRFact *projection =
                rir_scope_find_projection_fact(scope, local_name);
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
                        rir_scope_display_name(scope),
                        node->name != NULL ? node->name : "(unnamed)",
                        node->name != NULL ? node->name : "(unnamed)");
                }
                return false;
            }
        }
    }

    return true;
}
