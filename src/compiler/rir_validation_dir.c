#include "rir.h"
#include "rir_internal.h"

#include <string.h>

#include "dir.h"
#include "parser/ast_api.h"
#include "../common/string_compat.h"

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
rir_scope_name_matches(const RIRScope *scope,
                       const char *owner_name,
                       size_t owner_len)
{
    const char *scope_name = rir_scope_name(scope);
    return scope != NULL
        && scope_name != NULL
        && owner_name != NULL
        && strlen(scope_name) == owner_len
        && strncmp(scope_name, owner_name, owner_len) == 0;
}

static const RIRScope *
rir_find_domain_scope_for_owner(const RIRProgram *rir,
                                const char *owner_name,
                                size_t owner_len)
{
    RIRScopeInventory inventory;
    rir_scope_inventory_from_program(rir, &inventory);
    for (size_t i = 0; i < inventory.count; i++) {
        const RIRScope *scope = rir_scope_inventory_get(&inventory, i);
        if (scope == NULL)
            continue;
        if ((rir_scope_kind(scope) == RIR_SCOPE_ZONE
             || rir_scope_kind(scope) == RIR_SCOPE_RELATION
             || rir_scope_kind(scope) == RIR_SCOPE_EFFECT)
            && rir_scope_name_matches(scope, owner_name, owner_len)) {
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

        if (!rir_split_qualified_name(node->name, &owner_name,
                                      &owner_len, &local_name)) {
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
